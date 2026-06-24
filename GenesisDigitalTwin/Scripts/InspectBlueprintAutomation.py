import unreal

names = [
    "BlueprintEditorLibrary", "BlueprintFactory", "SubobjectDataSubsystem",
    "AddNewSubobjectParams", "SubobjectDataHandle", "K2Node_CallFunction",
    "K2Node_Timeline", "K2Node_IfThenElse", "K2Node_ExecutionSequence",
]

report = []
for name in names:
    obj = getattr(unreal, name, None)
    report.append("{0}: {1}\n{2}".format(name, obj, getattr(obj, "__doc__", "")))
    if obj:
        report.append("  methods: {0}".format(", ".join(sorted([x for x in dir(obj) if not x.startswith("_")]))))

report.append("K2 candidates: " + ", ".join(sorted(x for x in dir(unreal) if x.startswith("K2Node_"))))
raise RuntimeError("\n".join(report))
