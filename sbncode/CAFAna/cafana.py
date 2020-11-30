if __name__ == '__main__':
    print("This is the source file that implements the cafana python module.")
    print("You should 'import cafana' at the top of a .py script")
    print("and execute it with 'cafe myscript.py'")
    exit(1)

import os
inc = os.environ['MRB_INSTALL']+'/sbncode/'+os.environ['SBNCODE_VERSION']+'/include/'
os.environ['ROOT_INCLUDE_PATH'] = \
  ':'.join([inc,
            inc+'sbncode',
            inc+'sbncode/CAFAna',
            os.environ['SRPROXY_INC']])

print(os.environ['ROOT_INCLUDE_PATH'])

import ROOT

print('Load libs...')
for lib in ['Minuit2',
            'StandardRecordProxy',
            'CAFAnaCore',
            'SBNAnaVars',
            'SBNAnaCuts',
            'CAFAnaSysts',
            'CAFAnaExtrap',
            'CAFAnaPrediction',
            'CAFAnaExperiment',
            'CAFAnaAnalysis']:
    print(' ', lib)
    ROOT.gSystem.Load('lib'+lib+'.so')


import cppyy

print('Load dict...')
cppyy.load_reflection_info('libCAFAna_dict.so')

class PyCAFAna:
    def __init__(self, cppyy):
        self._cppyy = cppyy

    def CSliceVar(self, body):
        '''Construct a new slice Var given the C++ body as a string'''
        var = 'pyvar_'+self._cppyy.gbl.ana.UniqueName()
        text = '#include "StandardRecord/Proxy/SRProxy.h"\ndouble '+var+'_func(const caf::SRSliceProxy* srp){\nconst caf::SRSliceProxy& sr = *srp;\n'+body+'\n}\nconst ana::Var '+var+'('+var+'_func);'
        self._cppyy.cppdef(text)
        return getattr(self._cppyy.gbl, var)

    def CSpillVar(self, body):
        '''Construct a new spill Var given the C++ body as a string'''
        var = 'pyvar_'+self._cppyy.gbl.ana.UniqueName()
        text = '#include "StandardRecord/Proxy/SRProxy.h"\ndouble '+var+'_func(const caf::SRSpillProxy* srp){\nconst caf::SRSpillProxy& sr = *srp;\n'+body+'\n}\nconst ana::Var '+var+'('+var+'_func);'
        self._cppyy.cppdef(text)
        return getattr(self._cppyy.gbl, var)

# TODO simplevar. TODO cuts

    # If we don't provide it explicitly, assume it's from the ana namespace
    def __getattr__(self, name):
        return getattr(self._cppyy.gbl.ana, name)

import sys
sys.modules['cafana'] = PyCAFAna(cppyy)
