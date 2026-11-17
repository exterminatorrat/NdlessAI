// Line-level OCR with boxes, for pages whose content is drawn rather than typed.
//
//   apchem_ocr page1.png page2.png …
//   apchem_ocr --fast - < pages.txt
//   {"path":"page1.png","w":1275,"h":1651,"lines":[{"t":"(A) Na2O","x":67,"y":527,"w":80,"h":14}]}
//
// The College Board bank rasterizes exactly the parts a chemistry reader needs
// most — equations, tables, Lewis structures, graphs — so the PDF text layer
// alone reads "(A) (B) (C) (D)" where four balanced equations should be. This
// reports one record per recognised text line with the geometry `apchem_ingest`
// needs to put a page back together: line boxes let a page be split into items
// by position instead of by guessing at newlines, and let the accurate text
// layer win over OCR wherever the page also typed the same run.
//
// It takes a list and holds one recogniser for the whole list, because spawning
// a process per page wedged the Vision service after a few hundred pages and
// left the run stuck rather than failing. Each page is printed and flushed as
// soon as it is read, so a killed process still yields every page it finished.
//
// macOS only, because it is Vision's on-device recogniser and nothing else has
// to be installed to get it. Coordinates are pixels from the top left.

import Foundation
import Vision
import AppKit

func fail(_ message: String) -> Never {
    FileHandle.standardError.write((message + "\n").data(using: .utf8)!)
    exit(1)
}

var level: VNRequestTextRecognitionLevel = .accurate
let arguments = Array(CommandLine.arguments.dropFirst())
var paths: [String] = []
for argument in arguments {
    if argument == "--fast" { level = .fast; continue }
    if argument == "-" { paths.append(argument); continue }
    paths.append(argument)
}
if paths == ["-"] {
    var input = ""
    while let chunk = readLine(strippingNewline: false) {
        input += chunk
    }
    paths = input.split(separator: "\n").map(String.init).filter { !$0.isEmpty }
}
if paths.isEmpty { fail("usage: apchem_ocr [--fast] page.png … | apchem_ocr [--fast] - < pages.txt") }
request.recognitionLevel = level

// `.accurate` runs the text model on the Neural Engine, which on this machine
// starts returning `e5rtError(..., 13)` after a few hundred pages and then keeps
// failing until the service is reset by a reboot. `.fast` runs the same
// recogniser on a smaller model with no ANE stream, reads these pages at
// 0.25 s each, and on an equation-and-table probe page recovered the same
// content. So accurate is preferred while it works and fast is the floor.
var request = VNRecognizeTextRequest()
request.usesLanguageCorrection = false

for path in paths {
    guard let image = NSImage(contentsOfFile: path),
          let tiff = image.tiffRepresentation,
          let bitmap = NSBitmapImageRep(data: tiff),
          let cgImage = bitmap.cgImage else {
        fail("cannot load \(path)")
    }
    let width = Double(cgImage.width)
    let height = Double(cgImage.height)

    var attempts = 0
    var recognised = false
    while !recognised {
        let handler = VNImageRequestHandler(cgImage: cgImage, options: [:])
        do {
            try handler.perform([request])
            recognised = true
        } catch {
            attempts += 1
            if level == .accurate {
                FileHandle.standardError.write(
                    "accurate recognition unavailable, using fast: \(error)\n"
                        .data(using: .utf8)!)
                level = .fast
                request.recognitionLevel = level
                continue
            }
            // The Espresso runtime behind this refuser is intermittently
            // exhausted on this machine. Three attempts with a widening pause
            // either ride it out or mark the page, and a marked page never stops
            // the pages behind it.
            if attempts >= 3 {
                let payload: [String: Any] = ["path": path, "failed": true, "lines": []]
                let data = try! JSONSerialization.data(withJSONObject: payload, options: [])
                FileHandle.standardOutput.write(data)
                FileHandle.standardOutput.write("\n".data(using: .utf8)!)
                FileHandle.standardError.write(
                    "vision failed on \(path): \(error)\n".data(using: .utf8)!)
                break
            }
            Thread.sleep(forTimeInterval: Double(attempts * 4))
        }
    }
    if !recognised { continue }

    var lines: [[String: Any]] = []
    for observation in request.results ?? [] {
        guard let candidate = observation.topCandidates(1).first else { continue }
        let text = candidate.string.trimmingCharacters(in: .whitespacesAndNewlines)
        if text.isEmpty { continue }
        // Vision reports a normalised box with the origin at the bottom left.
        let box = observation.boundingBox
        lines.append([
            "t": text,
            "x": Int((box.minX * width).rounded()),
            "y": Int(((1.0 - box.maxY) * height).rounded()),
            "w": Int((box.width * width).rounded()),
            "h": Int((box.height * height).rounded()),
        ])
    }

    lines.sort { left, right in
        let ly = left["y"] as! Int, ry = right["y"] as! Int
        if abs(ly - ry) > 4 { return ly < ry }
        return (left["x"] as! Int) < (right["x"] as! Int)
    }

    let payload: [String: Any] = [
        "path": path, "w": Int(width), "h": Int(height), "lines": lines,
    ]
    let data = try! JSONSerialization.data(withJSONObject: payload, options: [])
    FileHandle.standardOutput.write(data)
    FileHandle.standardOutput.write("\n".data(using: .utf8)!)
}
