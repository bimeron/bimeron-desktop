"""EXPLORATORY MODEL only: assumed closure, no native evidence producer or writes.

Finite symbols stand for exact authenticated inputs/inventories, not a storage
schema. Atomic steps assume shared serialization, crash-persistent exclusion and
complete migration inventory. Terminal evidence ASSUMES enforceable closure of
the entire domain; no finite observation establishes that assumption. Successful
enrollment removal, administrative recovery and production authority are absent.
"""
from dataclasses import dataclass, replace
from itertools import product
import inspect


# Each tuple enumerates the complete bounded world, including future routes.
DOMAIN = ("GUI/policy", "Core/service", "payload", "native-owner",
          "packet-start", "signed-updater", "installer", "cached-old-host",
          "nested-removal", "rollback", "service-restoration", "commit",
          "delayed-server", "reboot")
INPUTS = ("MSI/identity", "effective-database/Binary", "architecture",
          "Core/GUI-digests", "external-payload-digests", "protected-preload",
          "allowed-properties")
BASE = ("no-policy", "original-service", "original-payload")
TARGET = ("enrolled-policy", "new-service", "new-payload")
UPDATED = ("updated-policy", "updated-service", "updated-payload")
ASSUMPTIONS = ("MODEL authorized complete no-predecessor observation",
               "MODEL all old/cached hosts fenced or participating",
               "MODEL serialized durable exclusion survives publisher death")
CLOSURE = "MODEL assumed enforceable closure; NO native producer"


@dataclass(frozen=True)
class Binding:
    installation: str = "installation-A"
    generation: int = 1
    operation: str = "enroll"
    inputs: tuple = INPUTS
    predecessor: int = 0  # Explicit no-predecessor symbol, never missing state.
    baseline: tuple = BASE
    target: tuple = TARGET
    domain: tuple = DOMAIN


@dataclass(frozen=True)
class Event:
    kind: str
    binding: Binding = Binding()
    work: str = "installer"
    assumptions: tuple = ASSUMPTIONS


@dataclass(frozen=True)
class Terminal:
    binding: Binding
    outcome: str
    expected: tuple
    observed: tuple
    domain: tuple = DOMAIN
    outstanding: tuple = ()
    closure: str = CLOSURE


@dataclass(frozen=True)
class Model:
    state: str = "unenrolled"
    inventory: tuple = BASE
    settled: Binding = None
    admitted: Binding = None
    closed: bool = False  # NEW admissions closed; existing work may still need drain.
    durable: bool = False
    seen: tuple = ()
    active: bool = False
    history: bool = False


def step(m, e):
    """Refusal leaves state unchanged. Uncertainty is sticky, never absence."""
    if isinstance(e, Terminal):
        b = m.admitted
        if m.state != "pending" or e.binding != b or not m.durable:
            return m
        expected = b.target if e.outcome == "commit" else b.baseline
        if (e.outcome not in ("commit", "rollback") or
                e.expected != expected or e.observed != expected or
                m.inventory != expected or e.domain != b.domain or
                e.outstanding or e.closure != CLOSURE):
            return m
        first_rollback = b.operation == "enroll" and e.outcome == "rollback"
        return replace(m, state="unenrolled" if first_rollback else "settled",
                       settled=m.settled if e.outcome == "rollback" else b,
                       admitted=None, closed=False, durable=False)
    b = e.binding
    if e.kind in ("read-error", "corrupt", "crash", "publication-failure"):
        return replace(m, state="unsafe", closed=True)
    if e.kind == "absence":
        return replace(m, state="unsafe", closed=True) if m.history else m
    if e.kind == "admit-new":
        if m.closed or m.state != "settled" or b != m.settled:
            return m
        return replace(m, active=True)
    if e.kind == "drain":
        return replace(m, active=False)
    if e.kind == "close":
        first = (m.state == "unenrolled" and b.operation == "enroll" and
                 b.predecessor == 0 and b.baseline == BASE)
        later = (m.state == "settled" and b.operation == "modify" and
                 b.installation == m.settled.installation and
                 b.predecessor == m.settled.generation and
                 b.baseline == m.inventory)
        if (m.closed or m.active or not (first or later) or
                b.generation not in (1, 2) or b.generation in m.seen or
                b.inputs != INPUTS or b.domain != DOMAIN or
                b.target not in (TARGET, UPDATED) or e.assumptions != ASSUMPTIONS):
            return m
        return replace(m, admitted=b, closed=True,
                       seen=m.seen + (b.generation,))
    if e.kind == "publish":
        if m.closed and m.admitted == b and m.state in ("unenrolled", "settled"):
            return replace(m, state="pending", durable=True, history=True)
        return m
    if e.kind in ("mutate", "rollback", "restore-service", "nested", "late-server"):
        if (m.state != "pending" or not m.closed or not m.durable or
                b != m.admitted or e.work not in b.domain):
            return m
        inventory = b.baseline if e.kind in ("rollback", "restore-service") else b.target
        return replace(m, inventory=inventory)
    # callback/exit/ACK/checkpoint (even combined), retry and delete prove nothing.
    return m


def invariant(m):
    assert not (m.active and m.state == "pending"), "mutation domain not drained"
    if m.inventory != BASE:
        assert m.history, "mutation without durable Pending publication"
    if m.state == "pending":
        assert m.closed and m.durable and m.admitted is not None
    if m.state == "unsafe":
        assert m.closed, "uncertainty opened admissions"


def run(m, *events):
    for event in events:
        before = m
        m = step(m, event)
        invariant(m)
        if m.inventory != before.inventory:
            assert before.state == "pending" and before.durable and before.closed
            assert not before.active, "mutation overlaps conflicting work"
    return m


def focused_checks(transition=step):
    """Independent effects/refusals, also run against changed transition code."""
    def apply(m, *events):
        for event in events:
            before = m
            m = transition(m, event)
            invariant(m)
            if m.inventory != before.inventory:
                assert before.state == "pending" and before.closed and before.durable
                assert not before.active
        return m

    b = Binding()
    u = Model()
    closed = apply(u, Event("close"))
    pending = apply(closed, Event("publish"))
    mutated = apply(pending, Event("mutate"))
    settled = apply(mutated, Terminal(b, "commit", TARGET, TARGET))
    active = apply(settled, Event("admit-new"))
    modify = replace(b, generation=2, operation="modify", predecessor=1,
                     baseline=TARGET, target=UPDATED)
    assert apply(active, Event("close", modify)) == active
    assert apply(closed, Event("admit-new")) == closed
    for before in (u, closed):
        assert apply(before, Event("mutate")) == before
    for failure in ("crash", "publication-failure", "read-error", "corrupt", "absence"):
        unsafe = apply(active, Event(failure))
        assert unsafe.state == "unsafe" and unsafe.closed and unsafe.active
        assert apply(unsafe, Event("admit-new"), Event("mutate"),
                     Event("close", modify), Event("publish", modify)) == unsafe
        drained = apply(unsafe, Event("drain"))
        assert not drained.active and drained.state == "unsafe" and drained.closed
    # Every handler must visibly change inventory on its own. Test both
    # enrollment and modification; no preceding rollback can mask a missing one.
    for binding, origin in ((b, u), (modify, settled)):
        p = apply(origin, Event("close", binding), Event("publish", binding))
        changed = apply(p, Event("mutate", binding))
        for kind, work, before, expected in (
                ("mutate", "installer", p, binding.target),
                ("rollback", "rollback", changed, binding.baseline),
                ("restore-service", "service-restoration", changed, binding.baseline),
                ("nested", "nested-removal", p, binding.target),
                ("late-server", "delayed-server", p, binding.target)):
            event = Event(kind, binding, work)
            after = apply(before, event)
            assert after == replace(before, inventory=expected), kind
            assert after.inventory != before.inventory, kind
            assert after.closed and after.durable and not after.active
            bad_bindings = [replace(binding, **change) for change in (
                {"installation": "unrelated"}, {"generation": 0},
                {"operation": "other"}, {"inputs": ()}, {"predecessor": -1},
                {"baseline": ()}, {"target": ()}, {"domain": DOMAIN[:-1]})]
            for bad in bad_bindings:
                assert apply(before, replace(event, binding=bad)) == before, kind
            assert apply(before, replace(event, work="unrelated")) == before, kind
            for excluded in (origin, apply(origin, Event("close", binding)),
                             apply(changed, Terminal(
                    binding, "commit", binding.target, binding.target)),
                    apply(before, Event("crash"))):
                assert apply(excluded, event) == excluded, kind


def mutation_checks():
    """Compile single source changes in memory; never construct invalid states.

    Only step's source is read. Each mutant must fail the same focused behavioral
    checks as the real transition. Compilation/setup errors are not detections.
    """
    source = inspect.getsource(step)
    handlers = '("mutate", "rollback", "restore-service", "nested", "late-server")'
    changes = []
    for kind in ("mutate", "rollback", "restore-service", "nested", "late-server"):
        remaining = tuple(k for k in ("mutate", "rollback", "restore-service",
                                      "nested", "late-server") if k != kind)
        changes.append((f"remove-{kind}", handlers, repr(remaining)))
    changes += [
        ("remove-binding", 'b != m.admitted or ', ''),
        ("remove-domain", 'e.work not in b.domain', 'False'),
        ("remove-drain-gate", 'm.closed or m.active or not (first or later)',
         'm.closed or not (first or later)'),
        ("remove-pending-gate", 'm.state != "pending" or not m.closed or not m.durable or',
         'False or'),
        ("clear-active-on-uncertainty", 'state="unsafe", closed=True',
         'state="unsafe", closed=True, active=False'),
    ]
    for name, old, new in changes:
        assert old in source, name
        namespace = dict(globals())
        exec(compile(source.replace(old, new), f"<model-mutant:{name}>", "exec"), namespace)
        try:
            focused_checks(namespace["step"])
        except AssertionError:
            pass
        else:
            raise AssertionError(f"undetected transition regression: {name}")
    return len(changes)


def main():
    import sys
    if sys.flags.optimize:
        raise SystemExit("MODEL/assumed closure: run without -O; assertions required")
    b = Binding()
    u = Model()
    closed = run(u, Event("close"))
    pending = run(closed, Event("publish"))
    mutated = run(pending, Event("mutate"))
    commit = Terminal(b, "commit", TARGET, TARGET)
    settled = run(mutated, commit)
    assert settled.state == "settled" and settled.inventory == TARGET
    for before_publication in (u, closed):
        assert run(before_publication, Event("mutate")) == before_publication
    assert run(settled, commit, Event("late-server"), Event("publish")) == settled
    assert run(u, Event("absence")) == u
    # Packet-only operation is deliberately outside enrolled admission: no new
    # missing-state gate. Trusted unenrolled remains native-ineligible.
    assert u.state == "unenrolled" and run(u, Event("admit-new")) == u
    assert run(u, Event("packet-install"), Event("packet-start"),
               Event("signed-updater-rollback")) == u
    for baseline in ((), ("fabricated",)):
        assert run(u, Event("close", replace(b, baseline=baseline))) == u
    assert run(u, Event("close", assumptions=())) == u
    focused_checks()
    restored = run(mutated, Event("rollback"), Event("restore-service"),
                   Terminal(b, "rollback", BASE, BASE))
    assert restored.state == "unenrolled" and restored.inventory == BASE
    assert run(restored, Event("close")) == restored  # generation consumed
    assert run(restored, Event("absence")).state == "unsafe"
    modified_binding = replace(b, generation=2, operation="modify",
                               predecessor=1, baseline=TARGET, target=UPDATED)
    p2 = run(settled, Event("close", modified_binding), Event("publish", modified_binding))
    assert p2.state == "pending"
    m2 = run(p2, Event("mutate", modified_binding))
    rollback2 = run(m2, Event("rollback", modified_binding),
                    Event("restore-service", modified_binding),
                    Terminal(modified_binding, "rollback", TARGET, TARGET))
    assert rollback2.settled == b and rollback2.inventory == TARGET
    commit2 = run(m2, Terminal(modified_binding, "commit", UPDATED, UPDATED))
    assert commit2.settled == modified_binding and commit2.inventory == UPDATED
    assert run(commit2, Event("close", modified_binding),
               Event("late-server", modified_binding)) == commit2
    # A matching new admission may win first, forcing close to wait for drain.
    active = run(settled, Event("admit-new"))
    assert active.active and run(active, Event("close", modified_binding)) == active
    assert run(active, Event("drain"), Event("close", modified_binding)).closed
    assert run(pending, Event("admit-new")) == pending
    bad_bindings = [replace(b, **change) for change in (
        {"installation": "B"}, {"generation": 2}, {"operation": "other"},
        {"inputs": ()}, {"predecessor": 1}, {"baseline": ()},
        {"target": ()}, {"domain": DOMAIN[:-1]})]
    for bad in bad_bindings:
        assert run(pending, Event("mutate", bad), Event("rollback", bad),
                   Event("restore-service", bad), replace(commit, binding=bad)) == pending
    for bad in (replace(commit, expected=()), replace(commit, observed=()),
                replace(commit, binding=None),
                replace(commit, domain=DOMAIN[:-1]), replace(commit, outstanding=("reboot",)),
                replace(commit, closure="quiet snapshots"), replace(commit, outcome="remove")):
        assert run(mutated, bad) == mutated
    assert run(pending, Event("mutate", work="unrelated")) == pending
    assert run(mutated, *(Event(k) for k in
               ("callback", "exit-zero", "ACK", "checkpoint", "all-four"))) == mutated
    for at in (u, closed, pending, mutated, run(mutated, Event("rollback")),
               settled, p2, m2):
        for failure in ("crash", "publication-failure", "read-error", "corrupt"):
            unsafe = run(at, Event(failure))
            assert unsafe.state == "unsafe"
            assert run(unsafe, commit, Event("publish"), Event("retry"),
                       Event("delete"), Event("absence")).state == "unsafe"
    # Named omissions in migration obligations cannot be papered over by a new reader.
    assert run(u, Event("close", assumptions=ASSUMPTIONS[:1])) == u
    # Enumerate all length-five traces as before, now from reachable lifecycle
    # seeds, including active work and generation-two modification. No unbounded
    # proof or complete real-host inventory is implied by these finite symbols.
    alphabet = tuple(Event(k, binding) for binding in (b, modified_binding)
                     for k in ("close", "publish", "mutate", "admit-new", "drain",
                               "crash", "publication-failure", "read-error",
                               "corrupt", "absence", "rollback", "restore-service",
                               "nested", "late-server")) + (
        commit, Terminal(modified_binding, "commit", UPDATED, UPDATED),
        Terminal(b, "rollback", BASE, BASE),
        Terminal(modified_binding, "rollback", TARGET, TARGET))
    seeds = (u, settled, active, p2, m2)
    reached = set(seeds)
    frontier = set(seeds)
    edges = 0
    for _ in range(5):
        following = set()
        for state in frontier:
            for event in alphabet:
                following.add(run(state, event))
                edges += 1
        reached.update(following)
        frontier = following
    # Preserve the original single-seed trace count as a separate, narrow check.
    original = tuple(Event(k) for k in ("close", "publish", "mutate", "admit-new",
                                       "crash", "publication-failure")) + (commit,)
    for events in product(original, repeat=5):
        run(u, *events)
    mutants = mutation_checks()
    print("MODEL/assumed closure: PASS 16807 original length-5 traces; "
          f"5 reachable seeds, depth 5, {len(alphabet)} events, {edges} edges, "
          f"{len(reached)} unique states ({sum(m.active for m in reached)} active)")
    print(f"MODEL/assumed closure: PASS focused scenarios; {mutants} transition mutants caught")
    print("MODEL/assumed closure: NO native producer, durability, fencing or production acceptance")


if __name__ == "__main__":
    main()
