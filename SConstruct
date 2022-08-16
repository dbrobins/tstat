# Thermostat SCons build file
#
# D. Robins, 20190126

pathPigpio = '/usr/src/PIGPIO'

env = Environment()
env.Append(CPPPATH=[pathPigpio])
env.Append(LIBPATH=[pathPigpio])
env.Append(CCFLAGS=['-Wall', '-pthread', '-g']) #, '-O2'])
#env.Append(CPPDEFINES=['NDEBUG'])
env.Append(CXXFLAGS=['-std=c++17'])
env.Append(LIBS=['PocoFoundation', 'PocoUtil', 'PocoNet', 'pigpio'])
env.Append(LINKFLAGS=['-Wl,-rpath=' + pathPigpio])
env.Program('tstat', ['main.cc', 'tstat.cc'])

env.Program('pin', ['pin.cc'])
