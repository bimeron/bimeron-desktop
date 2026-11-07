"""Disconnected proposed-API algorithm fixture; NEVER production enablement.

All authorization/preload, protected staging, durability, fencing, migration,
exact inventory/domain closure and surviving bound authority are ASSUMED adapters.
Thread tokens model ownership, NOT Windows thread identity or native proof.
No subprocess, native calls, MSI, VM, fixture-state writes or recovery API.
"""
import inspect
from pathlib import Path
import runpy
from dataclasses import replace

if not __debug__:
    raise SystemExit("REFUSED: assertions required; run without -O")

# run_path avoids bytecode writes and does not run the reviewed model's main.
logical = runpy.run_path(str(Path(__file__).with_name("enrollment-model-test.py")))
Binding, BASE, TARGET = (logical[k] for k in ("Binding", "BASE", "TARGET"))


class Refusal(Exception):
    pass


class Assumed:
    """Injected return values/callbacks only; log includes assumed thread token."""
    def __init__(self, values=None, hook=None):
        self.values = values or {}
        self.hook = hook
        self.log = []
        self.thread = "assumed-owner"

    def call(self, event, binding):
        self.log.append((event, self.thread, binding))
        if self.hook:
            self.hook(event)
        value = self.values.get(event, "ok")
        if isinstance(value, Exception):
            raise value
        return value


class Controller:
    def __init__(self, adapter, binding=Binding()):
        self.a, self.b = adapter, binding
        self.owner = adapter.thread  # Assumption only, not get_ident/native pinning.
        self.guard = self.pending = self.admitted = self.dead = False
        self.state = "unenrolled"
        self.inventory = binding.baseline

    def call(self, event):
        if self.dead:
            raise Refusal("dead controller before boundary")
        if self.a.thread != self.owner:
            raise Refusal("wrong assumed owning thread")
        value = self.a.call(event, self.b)
        if self.dead:
            raise Refusal("dead controller after boundary")
        if self.a.thread != self.owner:
            raise Refusal("callback changed assumed owning thread")
        return value

    def require(self, event):
        value = self.call(event)
        if event == "guard-close-drain" and value == "proven-not-acquired":
            # Only an explicit, live verified no-acquisition result retires intent.
            self.guard = False
            self.state = "unenrolled"
        if value != "ok":
            raise Refusal(event)

    def gate(self, phase):
        self.require(phase + "-mandatory")
        # These are independent assumed proofs, never caller-supplied claims.
        for proof in ("process", "action-export-session", "preload-Binary-sequence",
                      "binding", "CAD-locator-only"):
            self.require(phase + ":" + proof)

    def continuation(self, binding, work):
        if (not self.pending or not self.admitted or binding != self.b or
                work not in self.b.domain or self.state != "pending"):
            return False
        # Assumed surviving authority permits admitted segments/restore after death.
        if self.dead:
            # Separate assumed surviving authority, NEVER live controller progress.
            if self.a.call("surviving-bound-authority", binding) != "ok":
                return False
        else:
            self.require("bound-authority")
        self.inventory = self.b.baseline if work in ("rollback", "service-restoration") else self.b.target
        return True

    def new_admission(self):
        return self.state == "unenrolled" and not self.guard and not self.dead

    def run(self):
        begun = ended = False
        try:
            for event in ("authenticate", "stage-outside-replaced-trees", "old-gated-migration"):
                self.require(event)
            self.guard = True  # Acquisition intent: a throwing callback may own it.
            self.state = "unsafe"
            self.require("guard-close-drain")
            self.require("Begin-no-existing-no-Join")
            begun = True
            self.state = "unsafe"  # Publication can be partial, even at write failure.
            for event in ("pending-write", "pending-flush", "pending-readback"):
                self.require(event)
            self.pending = True
            self.state = "pending"
            self.gate("immediate")
            self.gate("deferred")
            self.admitted = True
            result = self.call("Install-exact")
            if self.dead:
                raise Refusal("death cannot prove held-thread End completion")
            disposition = "COMMIT" if result == "ok" else "ROLLBACK"
            ended = True  # Never retry an uncertain End.
            self.require("End-" + disposition)
            if result not in ("ok", "install-failure"):
                raise Refusal("unsupported install outcome")
            self.require("rollback-enabled-no-reboot")
            expected = self.b.target if disposition == "COMMIT" else self.b.baseline
            observed = self.call("inventory-exact")
            if observed != expected:
                raise Refusal("inventory mismatch/incomplete/error")
            self.require("assumed-complete-domain-closure")
            for event in ("settle-write", "settle-flush", "settle-readback"):
                self.require(event)
            self.require("release-LAST")
            self.guard = False
            self.pending = self.admitted = False
            self.inventory = expected
            self.state = "settled"
            return "success" if result == "ok" else "safely-restored-failed-operation"
        except Exception:
            if begun and not ended and not self.dead:
                try:
                    self.require("End-ROLLBACK")
                except Exception:
                    pass  # Unknown End never clears exclusion or publication uncertainty.
            if self.guard:
                self.state = "unsafe"
            return "unresolved" if self.guard else "refused"


def check(controller=Controller):
    def run(values=None, hook=None):
        a = Assumed({"inventory-exact": TARGET, **(values or {})}, hook)
        c = controller(a)
        result = c.run()
        return c, a, result

    c, a, result = run()
    events = [e for e, _, _ in a.log]
    assert result == "success" and c.state == "settled" and not c.guard
    assert events == ["authenticate", "stage-outside-replaced-trees", "old-gated-migration",
                      "guard-close-drain", "Begin-no-existing-no-Join", "pending-write",
                      "pending-flush", "pending-readback", "immediate-mandatory",
                      *["immediate:" + p for p in ("process", "action-export-session", "preload-Binary-sequence", "binding", "CAD-locator-only")],
                      "deferred-mandatory",
                      *["deferred:" + p for p in ("process", "action-export-session", "preload-Binary-sequence", "binding", "CAD-locator-only")],
                      "Install-exact", "End-COMMIT", "rollback-enabled-no-reboot", "inventory-exact",
                      "assumed-complete-domain-closure", "settle-write", "settle-flush",
                      "settle-readback", "release-LAST"]
    assert all(t == c.owner and b == c.b for _, t, b in a.log)
    c, a, result = run({"Install-exact": "install-failure", "inventory-exact": BASE})
    assert result == "safely-restored-failed-operation" and c.inventory == BASE and not c.guard
    assert "End-ROLLBACK" in [e for e, _, _ in a.log]

    # Every API/adapter return error AND callback exception prevents settlement.
    for event in events:
        for failure in ("error", RuntimeError("injected callback/API exception")):
            c, a, result = run({event: failure})
            assert result not in ("success", "safely-restored-failed-operation"), event
            assert c.state != "settled", event
            if event != "release-LAST":
                assert "release-LAST" not in [e for e, _, _ in a.log], event
    for route in ("direct", "quiet-UI-skipped", "maintenance", "cached", "nested-old-removal"):
        for phase in ("immediate", "deferred"):
            for claim in ("missing", "conditioned", "property-bypass"):
                c, a, _ = run({phase + "-mandatory": route + ":" + claim})
                assert c.guard and c.state == "unsafe"
                assert "Install-exact" not in [e for e, _, _ in a.log]
            for proof in ("process", "action-export-session", "preload-Binary-sequence", "binding", "CAD-locator-only"):
                c, a, _ = run({phase + ":" + proof: "forged-PID-export-session-CAD-property-nonce-plan"})
                assert c.guard and c.state == "unsafe"
                assert "Install-exact" not in [e for e, _, _ in a.log]
    for event in events:
        def throwing_callback(e):
            if e == event:
                raise RuntimeError("callback exception")
        c, a, result = run(hook=throwing_callback)
        assert c.state != "settled" and result not in ("success", "safely-restored-failed-operation")
    for closure in ("outstanding", "out-of-domain", "callback-only", "exit-only", "unknown"):
        c, a, _ = run({"assumed-complete-domain-closure": closure})
        assert c.guard and "settle-write" not in [e for e, _, _ in a.log]
    for inventory in (BASE, (), "unknown", "access-denied"):
        c, a, _ = run({"inventory-exact": inventory})
        assert c.guard and "settle-write" not in [e for e, _, _ in a.log]
    for event in ("pending-write", "pending-flush", "pending-readback"):
        for end in ("ok", "unknown", "access-denied", RuntimeError("End")):
            c, a, _ = run({event: "partial-uncertain", "End-ROLLBACK": end})
            names = [e for e, _, _ in a.log]
            assert names[-1] == "End-ROLLBACK" and "Install-exact" not in names
            assert c.guard and c.state == "unsafe" and not c.new_admission()
            assert all(t == c.owner for _, t, _ in a.log)
    for result in ("unknown", "access-denied", "install-failure", "in-progress", "reboot-required", "reboot-initiated"):
        for end in ("End-COMMIT", "End-ROLLBACK"):
            c, a, _ = run({end: result, **({"Install-exact": "install-failure"} if end.endswith("ROLLBACK") else {})})
            assert c.guard and c.state == "unsafe"
            assert "settle-write" not in [e for e, _, _ in a.log]
        c, a, _ = run({"Install-exact": result})
        assert c.guard and c.state == "unsafe"
    for event in events:
        # Wrong-thread entry, callback continuation, End and release all refuse.
        a = Assumed({"inventory-exact": TARGET})
        a.hook = lambda e, event=event: setattr(a, "thread", "other") if e == event else None
        c = controller(a)
        assert c.run() not in ("success", "safely-restored-failed-operation")
        assert c.state != "settled"
    a = Assumed()
    c = controller(a)
    a.thread = "other"
    assert c.run() == "refused" and not a.log

    # Death after gate: already admitted segment and restore survive; NEW work fenced.
    a = Assumed()
    c = controller(a)
    survival = []
    def die(event):
        if event == "Install-exact":
            c.dead = True
            survival.append(not c.new_admission())
            survival.append(c.continuation(c.b, "nested-removal") and c.inventory == TARGET)
            survival.append(c.continuation(c.b, "service-restoration") and c.inventory == BASE)
            survival.append(not c.continuation(replace(c.b, generation=2), "rollback"))
            survival.append(not c.continuation(c.b, "unrelated"))
    a.hook = die
    assert c.run() == "unresolved" and c.guard and c.state == "unsafe"
    assert survival == [True] * 5
    assert not any(e.startswith("End-") or e == "settle-write" for e, _, _ in a.log)
    assert not c.continuation(c.b, "rollback")  # Unsafe is not a recovery API.
    c, a, _ = run()
    assert not c.continuation(c.b, "late-server")  # Closure fences late continuation.
    c = controller(Assumed())
    assert c.new_admission()  # Trusted Unenrolled packet compatibility only.
    assert not c.continuation(c.b, "rollback")


def boundary_check(controller=Controller):
    counts = {"death": 0, "failure": 0, "acquisition": 0}
    for rollback in (False, True):
        values = {"inventory-exact": BASE if rollback else TARGET,
                  "Install-exact": "install-failure" if rollback else "ok"}
        a = Assumed(values)
        assert controller(a).run() in ("success", "safely-restored-failed-operation")
        events = [e for e, _, _ in a.log]
        for event in events:
            # Dead before entry must not even invoke the injected boundary.
            a = Assumed(values)
            c = controller(a)
            c.dead = c.guard = True
            c.state = "unsafe"
            try:
                c.call(event)
            except Refusal:
                pass
            assert not a.log and c.guard and not c.new_admission(), event
            counts["death"] += 1
            a = Assumed(values)
            c = controller(a)
            def die(e):
                if e == event:
                    c.dead = True
            a.hook = die
            result = c.run()
            names = [e for e, _, _ in a.log]
            assert names[-1] == event, (event, names)
            assert result not in ("success", "safely-restored-failed-operation")
            assert c.state != "settled" and not c.new_admission()
            if events.index(event) >= events.index("guard-close-drain"):
                assert c.guard and c.state == "unsafe" and result == "unresolved"
            counts["death"] += 1
            for failure in ("error", RuntimeError("uncertain boundary completion")):
                a = Assumed({**values, event: failure})
                c = controller(a)
                assert c.run() not in ("success", "safely-restored-failed-operation")
                assert c.state != "settled"
                if events.index(event) >= events.index("guard-close-drain"):
                    assert c.guard and c.state == "unsafe" and not c.new_admission()
                counts["failure"] += 1
    for value, retained in (("proven-not-acquired", False), ("error", True),
                            (RuntimeError("may have acquired"), True)):
        a = Assumed({"guard-close-drain": value})
        c = controller(a)
        result = c.run()
        assert c.guard == retained and c.new_admission() == (not retained)
        assert c.state == ("unsafe" if retained else "unenrolled")
        assert result == ("unresolved" if retained else "refused")
        assert a.log[-1][0] == "guard-close-drain"
        counts["acquisition"] += 1
    # The error-cleanup End is not on either successful path.
    for failure in ("error", RuntimeError("cleanup End"), "death"):
        a = Assumed({"pending-readback": "partial-uncertain",
                     "End-ROLLBACK": "ok" if failure == "death" else failure})
        c = controller(a)
        a.hook = lambda e: setattr(c, "dead", True) if e == "End-ROLLBACK" and failure == "death" else None
        assert c.run() == "unresolved" and c.guard and c.state == "unsafe"
        assert a.log[-1][0] == "End-ROLLBACK" and not c.new_admission()
        counts["death" if failure == "death" else "failure"] += 1
    # Live continuation uses the same fence; surviving authority is separate.
    for dead in (False, True):
        event = "surviving-bound-authority" if dead else "bound-authority"
        for failure in ("error", RuntimeError("authority"), "death"):
            a = Assumed({event: "ok" if failure == "death" else failure})
            c = controller(a)
            c.guard = c.pending = c.admitted = True
            c.state = "pending"
            c.dead = dead
            a.hook = lambda e: setattr(c, "dead", True) if failure == "death" else None
            try:
                allowed = c.continuation(c.b, "nested-removal")
            except (Refusal, RuntimeError):
                allowed = False
            assert allowed == (dead and failure == "death")
            assert c.guard and c.state == "pending" and not c.new_admission()
            assert c.inventory == (TARGET if allowed else BASE)
            assert [e for e, _, _ in a.log] == [event]
            counts["death" if failure == "death" else "failure"] += 1
    # Acquisition callback may have taken ownership before it throws.
    a = Assumed()
    c = controller(a)
    acquired = []
    def acquire_then_throw(e):
        if e == "guard-close-drain":
            acquired.append(True)
            raise RuntimeError("acquired then callback threw")
    a.hook = acquire_then_throw
    assert c.run() == "unresolved" and acquired == [True]
    assert c.guard and c.state == "unsafe" and not c.new_admission()
    counts["acquisition"] += 1
    return counts


def main():
    check()
    counts = boundary_check()
    source = inspect.getsource(Controller)
    # Actual deletion/gate mutations run against the same behavioral oracle.
    mutations = [
        ('self.require("guard-close-drain")', 'pass'),
        ('self.gate("immediate")', 'pass'),
        ('self.gate("deferred")', 'pass'),
        ('self.require("End-" + disposition)', 'pass'),
        ('self.require("release-LAST")', 'pass'),
        ('self.require(event)', 'pass'),
        ('observed != expected', 'False'),
        ('binding != self.b', 'False'),
        ('if self.dead:', 'if False:'),
        ('self.a.thread != self.owner', 'False'),
        ('raise Refusal("dead controller before boundary")', 'pass'),
        ('raise Refusal("dead controller after boundary")', 'pass'),
        ('self.guard = True  # Acquisition intent: a throwing callback may own it.', 'pass'),
        ('value == "proven-not-acquired"', 'value != "ok"'),
    ]
    for old, new in mutations:
        assert old in source
        scope = dict(globals())
        exec(compile(source.replace(old, new), "<in-memory-mutant>", "exec"), scope)
        try:
            check(scope["Controller"])
            boundary_check(scope["Controller"])
        except AssertionError:
            continue
        raise AssertionError("surviving mutation: " + old)
    print("MODEL PASS: ordered commit/restored failure, API/callback/thread refusals,")
    print("publication uncertainty, gates/forgery and death continuation; 14 mutants caught.")
    print("BOUNDARY CASES:", counts)
    print("ASSUMED auth/durability/fencing/closure/thread/surviving authority; NO native proof.")


if __name__ == "__main__":
    main()
