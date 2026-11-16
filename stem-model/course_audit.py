"""Check the Tier 1 course map against the catalogue and print it.

    python3 course_audit.py                  # every course, claims checked
    python3 course_audit.py --course sat-act # one course
    python3 course_audit.py --gaps           # only what nothing answers yet

Exits 1 when the map lies: a relation claim that is not in `RELATIONS`, or a
typed claim whose form `pipeline_typed_answer` does not dispatch. That check is
the whole reason this is a script and not a table in a document - a served
topic that quietly points at a relation somebody renamed is worse than an
honest gap, because the plan then counts work that does not exist.
"""

import argparse
import sys

import courses
from relations import RELATIONS

# A typed claim that is not one of the dispatched forms is allowed only as a
# plain expression, because that is what the evaluator falls back to. It is
# counted separately so the difference stays visible.
EXPRESSION = "expression"


def classify(command):
    text = command.strip()
    for form in courses.TYPED_FORMS:
        if text.startswith(form):
            return text.split("(")[0].split("/")[0]
    return EXPRESSION


def check():
    """Every claim, as (problems, expression_typed)."""

    problems = []
    bare = 0
    for code, unit, topic in courses.iter_topics():
        kind, value = topic.serves
        if kind == "relation":
            if value not in RELATIONS:
                problems.append(f"{code} unit {unit}: {topic.topic!r} claims "
                                f"{value!r}, which is not a registered relation")
        elif kind == "typed":
            if not value:
                problems.append(f"{code} unit {unit}: {topic.topic!r} is typed "
                                f"with no command")
            elif classify(value) == EXPRESSION:
                bare += 1
        elif kind != "gap":
            problems.append(f"{code} unit {unit}: {topic.topic!r} has unknown "
                            f"claim kind {kind!r}")
    return problems, bare


def print_course(course, gaps_only):
    total, served, typed, gap = courses.counts(course.code)
    print(f"\n{course.name} ({course.code})")
    print(f"  {total} topics: {served} relation, {typed} typed, {gap} gap"
          f"  -> {served + typed} answered, {gap} missing")
    if course.exact_floor is not None:
        print(f"  hand-written floor {course.exact_floor:.1f}%  {course.note}")
    print()
    print(f"  {'unit':<6}{'topic':<52}{'how':<34}detail")
    for unit in course.units:
        for topic in unit.topics:
            kind, value = topic.serves
            if gaps_only and kind != "gap":
                continue
            how = {"relation": "relation", "typed": "typed",
                   "gap": "GAP"}[kind]
            print(f"  {unit.number:<6}{topic.topic[:50]:<52}{how:<34}"
                  f"{value}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--course", default="",
                    help="one course code; default every course")
    ap.add_argument("--gaps", action="store_true",
                    help="only the topics nothing answers yet")
    args = ap.parse_args()

    problems, bare = check()
    codes = [args.course] if args.course else list(courses.COURSES)
    for code in codes:
        if code not in courses.COURSES:
            raise SystemExit(f"unknown course {code!r}; have "
                             f"{', '.join(courses.COURSES)}")
        print_course(courses.COURSES[code], args.gaps)

    unclaimed = courses.unclaimed_relations()
    print()
    print(f"{len(courses.COURSES)} courses, "
          f"{sum(len(c.units) for c in courses.COURSES.values())} units, "
          f"{sum(1 for _ in courses.iter_topics())} topics; "
          f"{len(unclaimed)} of {len(RELATIONS)} relations serve a course "
          f"outside Tier 1")
    if bare:
        print(f"{bare} typed claims are plain expressions")

    if problems:
        print()
        for problem in problems:
            print(f"FAIL  {problem}")
        return 1
    print("OK    every relation claim is a registered relation")
    return 0


if __name__ == "__main__":
    sys.exit(main())
