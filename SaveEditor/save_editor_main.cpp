#define ALICE_NO_ENTRY_POINT 1
#include "common_types.cpp"
#ifdef _WIN64
#include "simple_fs_win.cpp"
#else
#include "simple_fs_nix.cpp"
#endif

int main(int argc, char** argv) {
	auto dir = simple_fs::get_or_create_oos_directory();
	if(argc <= 1) {
		std::printf("Usage: %s [file1] [file2]\n", argv[0]);
		return EXIT_FAILURE;
	}

	auto oos_file_1 = open_file(dir, simple_fs::utf8_to_native(argv[1]));
	if(!bool(oos_file_1))
		return EXIT_FAILURE;
	auto contents_1 = simple_fs::view_contents(*oos_file_1);
	auto const* start_1 = reinterpret_cast<uint8_t const*>(contents_1.data);
	auto end_1 = start_1 + contents_1.file_size;
	if(argc <= 2) {
		std::printf("Doing a report of %s\n", argv[1]);
		dcon::for_each_record(reinterpret_cast<const std::byte*>(start_1), reinterpret_cast<const std::byte*>(end_1), [&](dcon::record_header const& header_1, std::byte const* data_start_1, std::byte const* data_end_1) {
			auto size1 = data_end_1 - data_start_1;
			std::printf("%s.%s.%s,%zu\n", header_1.object_name_start, header_1.property_name_start, header_1.type_name_start, static_cast<size_t>(size1));
		});
	} else {
		auto oos_file_2 = open_file(dir, simple_fs::utf8_to_native(argv[2]));
		if(!bool(oos_file_2))
			return EXIT_FAILURE;
		auto contents_2 = simple_fs::view_contents(*oos_file_2);
		auto const* start_2 = reinterpret_cast<uint8_t const*>(contents_2.data);
		auto end_2 = start_2 + contents_2.file_size;

		std::printf("Comparing files %s and %s\n", argv[1], argv[2]);

		dcon::for_each_record(reinterpret_cast<const std::byte*>(start_1), reinterpret_cast<const std::byte*>(end_1), [&](dcon::record_header const& header_1, std::byte const* data_start_1, std::byte const* data_end_1) {
			dcon::for_each_record(reinterpret_cast<const std::byte*>(start_2), reinterpret_cast<const std::byte*>(end_2), [&](dcon::record_header const& header_2, std::byte const* data_start_2, std::byte const* data_end_2) {
				auto obj1 = std::string_view{ header_1.object_name_start, header_1.object_name_end };
				auto obj2 = std::string_view{ header_2.object_name_start, header_2.object_name_end };
				if(obj1 == obj2) {
					auto pop1 = std::string_view{ header_1.property_name_start, header_1.property_name_end };
					auto pop2 = std::string_view{ header_2.property_name_start, header_2.property_name_end };
					if(pop1 == pop2) {
						auto size1 = data_end_1 - data_start_1;
						auto size2 = data_end_2 - data_start_2;
						auto offset1 = data_start_1 - reinterpret_cast<const std::byte*>(start_1);
						auto offset2 = data_start_2 - reinterpret_cast<const std::byte*>(start_2);
						bool match = true;
						//
						if(size1 != size2) {
							std::printf("%s.%s.%s:", header_1.object_name_start, header_1.property_name_start, header_1.type_name_start);
							std::printf("Size mismatch (%u/%u)\n", static_cast<unsigned int>(offset1), static_cast<unsigned int>(offset2));
							match = false;
						}
						//
						if(std::memcmp(data_start_1, data_start_2, std::min(size1, size2))) {
							std::printf("%s.%s.%s:", header_1.object_name_start, header_1.property_name_start, header_1.type_name_start);
							std::printf("Data mismatch (%u/%u)", static_cast<unsigned int>(offset1), static_cast<unsigned int>(offset2));

							auto p1 = data_start_1;
							auto p2 = data_start_2;
							for(; *p1 == *p2; p1++, p2++);
							std::printf("<@+%u> ->\n", static_cast<unsigned int>(p1 - data_start_1));

							std::printf("<");
							for(int32_t i = -8; i < 8; i++)
								std::printf("%hhx ", static_cast<unsigned char>(p1[i]));
							std::printf(">\n<");
							for(int32_t i = -8; i < 8; i++)
								std::printf("%hhx ", static_cast<unsigned char>(p2[i]));
							std::printf(">\n");

							match = false;
						}
						//
						if(!match) {
							std::printf("*NOT MATCHING*\n");
						}
					}
				}
			});
		});
	}
	std::printf("Kosher! Finished! ^-^\n");
	return EXIT_SUCCESS;
}
