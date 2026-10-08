# Local Phase 0G.1 corrections

Preserved accepted 8-bit stage allocation and transparent RAM compositing, and added required forward declarations. Removed remaining LowCharge/Unstable random patrol station routing; those rules used visual-state thresholds above the intended autonomous need thresholds. Ordinary awake patrol now selects only random field targets. Needs retain Charge <=20, Integrity <=25, Signal <=22, Mood <=30, with validation logs.
