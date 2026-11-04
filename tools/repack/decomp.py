import os, sys
TOOLS = r"C:\Users\somtam\.claude\tools"
os.environ["GHIDRA_INSTALL_DIR"] = TOOLS + r"\ghidra_12.1.3_PUBLIC"
os.environ["JAVA_HOME"] = TOOLS + r"\jdk-21"
import pyghidra
pyghidra.start()
from ghidra.base.project import GhidraProject
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
prog_name=sys.argv[1]
project = GhidraProject.openProject(r"C:\Users\somtam\CLAUDE~1\tools\ghidra-projects\d2", "d2", True)
program = project.openProgram("/", prog_name, True)
af = program.getAddressFactory(); fm = program.getFunctionManager()
dec = DecompInterface(); dec.openProgram(program)
for a in sys.argv[2:]:
    addr = af.getAddress(a)
    f = fm.getFunctionContaining(addr)
    if f is None:
        print('== no function at', a); continue
    r = dec.decompileFunction(f, 60, ConsoleTaskMonitor())
    print('== %s (%s) entry %s' % (a, f.getName(), f.getEntryPoint()))
    print(r.getDecompiledFunction().getC() if r.decompileCompleted() else r.getErrorMessage())
    refs=[str(x.getFromAddress()) for x in program.getReferenceManager().getReferencesTo(f.getEntryPoint())]
    print('callers:', refs[:20])
project.close()
