#pragma once
#ifndef LIBSNOW_HPP_RANU2LSO
#define LIBSNOW_HPP_RANU2LSO

namespace snow {
	void init(const char* runtime_bitcode_path);
	void finish();
	int main(int argc, const char** argv);
}

#endif /* end of include guard: LIBSNOW_HPP_RANU2LSO */
