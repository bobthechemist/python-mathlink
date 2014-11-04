# Mathlink bindings for Python #

Adapted to work with the Raspberry Pi version of Mathematica

1. Some notes
* python code is not present in the RPi version but can be found in a desktop 
installation
* make sure python-dev is installed

2. modify mathlink.c
* Add #define MLINTERFACE 3 above the #include lines

3. modify setup.py
* mathematicaversion = "10.0"
* if(re.search(r'linux2',sys.platform)):
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
	  
4. Not sure if this is the best way to obtain the necesssary results, but
* Copy /lib/arm-linux-gnueabihf/libuuid.so.1.3.0 to /usr/local/lib and rename to libuuid.so.  
* Copy /opt/Wolfram/WolframEngine/10.0/SystemFiles/Links/Mathlink/DeveloperKit/Linux-ARM/CompilerAdditions/libML32i3.so to /usr/local/lib
* Run sudo ldconfig

5. python setup.py build and sudo python setup.py install should work now

6. test with 
* python -c "import mathlink"
