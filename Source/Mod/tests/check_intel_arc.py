from pathlib import Path
import ctypes
r=Path(__file__).resolve().parent.parent
dll=ctypes.WinDLL(str(r/'IntelArc/x64/Release/Intel ARC.dll'))
f=dll.IntelArcMatchesAdapter;f.argtypes=[ctypes.c_uint,ctypes.c_wchar_p];f.restype=ctypes.c_int
cases=[(0x8086,'Intel(R) Arc(TM) A770 Graphics',True),(0x8086,'Intel(R) Arc(TM) B580 Graphics',True),(0x8086,'Intel Arc Pro A60',True),(0x8086,'Intel(R) UHD Graphics 770',False),(0x8086,'Intel Iris Xe Graphics',False),(0x10de,'NVIDIA GeForce RTX 4080',False),(0x1002,'AMD Radeon RX 7900',False),(0x10de,'Intel Arc A770',False),(0x8086,'Search Graphics',False),(0x8086,None,False)]
for vendor,name,expected in cases:assert bool(f(vendor,name))==expected,(vendor,name)
dll.IntelArcAllowsDevice.argtypes=[ctypes.c_void_p]
assert not dll.IntelArcAllowsDevice(None)
assert dll.IntelArcApiVersion()==1
print('PASS: Intel Arc A/B/Pro, non-Arc Intel, NVIDIA, AMD, vendor mismatch, invalid devices')
