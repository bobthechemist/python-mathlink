# Python 2.1.1 or newer setup style build information
# You will need to change the include_dirs and library_dirs
# entries to locations appropriate for your installation of
# the MathLink library.
import sys,re
from distutils.core import setup, Extension

mathematicaversion = "10.0"
pythonlinkversion = "0.0.4"

if(re.search(r'linux2',sys.platform)):
	setup(name="mathlink", version=pythonlinkversion,
		ext_modules=[
			Extension(
				"mathlink",
				["mathlink.c"],
				include_dirs = ["/opt/Wolfram/WolframEngine/" + mathematicaversion + "/SystemFiles/Links/MathLink/DeveloperKit/Linux-ARM/CompilerAdditions"],
				library_dirs = ["/opt/Wolfram/WolframEngine/" + mathematicaversion + "/SystemFiles/Links/MathLink/DeveloperKit/Linux-ARM/CompilerAdditions"],
				libraries = ["uuid", "ML32i3", "m", "pthread", "rt"]
			)
		]
	)

print "\nFinished building the mathlink extension module version %s." % (pythonlinkversion)
