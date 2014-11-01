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
elif(re.search(r'darwin', sys.platform)):
	setup(name="mathlink", version=pythonlinkversion,
		ext_modules=[
			Extension(
				"mathlink",
				["mathlink.c"],
				include_dirs = ["/Applications/Mathematica.app/SystemFiles/Links/MathLink/DeveloperKit/CompilerAdditions"],
				library_dirs = ["/Applications/Mathematica.app/SystemFiles/Links/MathLink/DeveloperKit/CompilerAdditions"],
				libraries = ["MLi3"],
			)
		]
	)
elif(not re.search(r'win32',sys.platform)):
	setup(name="mathlink", version=pythonlinkversion, 
		ext_modules=[
			Extension(
				"mathlink",
				["mathlink.c"],
				include_dirs = ["/usr/local/Wolfram/Mathematica/" + mathematicaversion + "/AddOns/MathLink/DeveloperKit/Linux/CompilerAdditions"],
				library_dirs = ["/usr/local/Wolfram/Mathematica/" + mathematicaversion + "/AddOns/MathLink/DeveloperKit/Linux/CompilerAdditions"],
				libraries = ["ML32i3","m"]
			)
		]
	)
elif(re.search(r'win32',sys.platform)):
	setup(name="mathlink",version=pythonlinkversion,
		ext_modules=[
			Extension(
				"mathlink",
				["mathlink.c"],
				include_dirs = ["C:\\Program Files\\Wolfram Research\\Mathematica\\" + mathematicaversion + "\\SystemFiles\\Links\\MathLink\\DeveloperKit\\Windows\\CompilerAdditions\\mldev32\\include"],
				library_dirs = ["C:\\Program Files\\Wolfram Research\\Mathematica\\" + mathematicaversion + "\\SystemFiles\\Links\\MathLink\\DeveloperKit\\Windows\\CompilerAdditions\\mldev32\\lib"],
				define_macros=[("WINDOWS_MATHLINK",None)],
				libraries = ["ml32i3m"]
			)
		]
	)

print "\nFinished building the mathlink extension module version %s." % (pythonlinkversion)
