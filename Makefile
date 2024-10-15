CXXFLAGS        += -Wall -Wextra -pedantic -Werror
CFLAGS        += -Wall -Wextra -pedantic -Werror

ntsc-kit-test: ntsc-kit-test.o ntsc-kit.o
	$(CXX) $(CXXFLAGS) $^ -o $@

