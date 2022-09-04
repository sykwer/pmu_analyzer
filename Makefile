lib/libpmutracer.so:
	mkdir -p lib
	g++ -shared -fPIC ./src/pmu.cpp -lpfm -Iinclude -o lib/libpmuanalyzer.so
