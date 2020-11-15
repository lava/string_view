#include <bev/string_view.hpp>

int main() {
	// TODO: Somehow run the existing libstdc++ string_view test suite against this class.
	std::string foo("asdf");
	bev::string_view sv{foo};
	return foo == sv;
}
