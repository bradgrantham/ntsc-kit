ntsc-kit-test: ntsc-kit-test.o ntsc-kit.o
	$(CXX) $(CXXFLAGS) $^ -o $@

