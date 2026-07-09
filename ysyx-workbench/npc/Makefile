all:
	@echo "Write this Makefile by your self."
sim:
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	verilator -cc --exe --build --trace-fst -j vsrc/example.v csrc/main.cpp
	obj_dir/Vexample
include ../Makefile