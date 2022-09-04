lib/libpmutracer.so:
	mkdir -p lib
	g++ -shared -fPIC -g ./src/pmu.cpp -lpfm -lyaml-cpp -Iinclude -o lib/libpmuanalyzer.so
