# -*- python -*-
## 2010, Copyright University Corporation for Atmospheric Research

import os
import eol_scons

# eol_scons.debug.SetDebug("true")
#
# Don't load tools that perform compiler or pkg-config checks
# until the cross tool is loaded, and PKG_CONFIG_PATH is set.
env = Environment(tools=['default', 'qt4', 'qtgui', 'qtcore', 'qtnetwork', 'nidas', 'gsl'])

opts = eol_scons.GlobalVariables('config.py')
opts.AddVariables(('PREFIX',
                   'installation path',
                   '/opt/nidas', None, eol_scons.PathToAbsolute))

opts.Update(env)

# Override CXXFLAGS in order to turn off -Weffc++ for now
env['CXXFLAGS'] = [ '-Wall','-O2' ]

sources = Split("""
    main.cc
    TreeItem.cc
    TreeModel.cc
    AutoCalClient.cc
    CalibrationWizard.cc
    Calibrator.cc
""")

auto_cal = env.NidasProgram('auto_cal', sources)

name = env.subst("${TARGET.filebase}", target=auto_cal)

inode = env.Install('$PREFIX/bin', auto_cal)
env.Clean('install', inode)
