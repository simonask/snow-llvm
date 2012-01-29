#include "snow/array.hpp"
#include "snow/map.hpp"
#include "snow/exception.hpp"
#include "snow/symbol.hpp"
#include "snow/function.hpp"

#include <map>
#include <vector>

namespace snow {
	namespace {
		enum SerializedObjectType {
			ArrayType = AnyType+1,
			MapType,
			ObjectReferenceType
		};
	
		template <typename Base>
		struct Serializer : Base {
			std::map<Value, size_t> object_map;
		
			template <typename... Args>
			Serializer(Args... args) : Base(args...) {}
		
			void write(uint8_t u8) {
				this->write_byte(u8);
			}
		
			void write(uint64_t u64) {
				uint8_t* p = reinterpret_cast<uint8_t*>(&u64);
				for (size_t i = 0; i < sizeof(u64); ++i) {
					write(p[i]);
				}
			}
		
			void write(ValueType type) { write((uint8_t)type); }
			void write(SerializedObjectType type) { write((uint8_t)type); }
		
			void serialize(Value value) {
				auto it = object_map.find(value);
				if (it != object_map.end()) {
					write_object_ref(it->second);
				} else {
					if (value.is_immediate()) {
						serialize_immediate(value);
					} else if (get_class(value) == get_array_class()) {
						serialize_array(value);
					} else if (get_class(value) == get_map_class()) {
						serialize_map(value);
					} else {
						throw_exception_with_description("Marshal: Can only serialize simple types, arrays and maps (got %@).", value_inspect(value));
					}
				}
			}
			
			void serialize_array(ArrayConstPtr array) {
				object_map[array] = this->get_offset();
				write(ArrayType);
				write((uint64_t)array_size(array));
				for (size_t i = 0; i < array_size(array); ++i) {
					serialize(array_get(array, i));
				}
			}
			
			void serialize_map(MapConstPtr map) {
				object_map[map] = this->get_offset();
				write(MapType);
				size_t sz = map_size(map);
				write((uint64_t)sz);
				SN_STACK_ARRAY(KeyValuePair, pairs, sz);
				map_get_pairs(map, pairs, sz);
				for (size_t i = 0; i < sz; ++i) {
					serialize(pairs[i].pair[0]);
					serialize(pairs[i].pair[1]);
				}
			}
			
			void serialize_immediate(Immediate imm) {
				size_t offset = this->get_offset();
				write(imm.type());
				if (imm.is_symbol()) {
					object_map[imm] = offset;
					const char* str = sym_to_cstr(value_to_symbol(imm));
					for (const char* p = str; *p; ++p) {
						write((uint8_t)*p);
					}
					write((uint8_t)0);
				} else {
					write((uint64_t)imm.value());
				}
			}
		
			void write_object_ref(size_t offset) {
				write(ObjectReferenceType);
				write((uint64_t)offset);
			}
		};
	
		struct BufferWrite {
			std::vector<byte>& buffer;
			
			BufferWrite(std::vector<byte>& buffer) : buffer(buffer) {}
			size_t get_offset() { return buffer.size(); }
			void write_byte(uint8_t u8) { buffer.push_back(u8); }
		};
		
		template <typename Base>
		struct Deserializer : Base {
			std::map<size_t, Value> object_map;
			
			template <typename... Args>
			Deserializer(Args... args) : Base(args...) {}
			
			template <typename T>
			void read(T& out) {
				byte* p = reinterpret_cast<byte*>(&out);
				for (size_t i = 0; i < sizeof(T); ++i) {
					if (!this->read_byte(p[i])) {
						throw_exception_with_description("Marshal: Incomplete data for deserialization.");
					}
				}
			}
			
			Value deserialize() {
				byte type;
				read(type);
				if (type < ValueTypeMask) {
					ValueType t = (ValueType)type;
					if (is_immediate_type(t)) {
						return deserialize_immediate(t);
					} else {
						throw_exception_with_description("Marshal: Corrupt data (wrong object type at offset %@).", this->get_offset()-1);
						return NULL;
					}
				}
				else if (type == ArrayType) {
					return deserialize_array();
				} else if (type == MapType) {
					return deserialize_map();
				} else if (type == ObjectReferenceType) {
					return deserialize_object_ref();
				} else {
					throw_exception_with_description("Marshal: Corrupt data (wrong object type at offset %@).", this->get_offset()-1);
					return NULL;
				}
			}
			
			Value deserialize_immediate(ValueType type) {
				if (type == SymbolType) {
					size_t offset = this->get_offset() - 1;
					std::vector<byte> buffer;
					byte b;
					read(b);
					while (b != 0) {
						buffer.push_back(b);
						read(b);
					}
					char str[buffer.size()+1];
					for (size_t i = 0; i < buffer.size(); ++i) {
						str[i] = buffer[i];
					}
					str[buffer.size()] = '\0';
					Value value = vsym(str);
					object_map[offset] = value;
					return value;
				} else {
					uintptr_t u;
					read(u);
					return (VALUE)u;
				}
			}
			
			Value deserialize_array() {
				size_t offset = this->get_offset() - 1;
				uint64_t sz;
				read(sz);
				ArrayPtr array = create_array_with_size(sz);
				object_map[offset] = array;
				for (size_t i = 0; i < sz; ++i) {
					Value val = deserialize();
					array_push(array, val);
				}
				return array;
			}
			
			Value deserialize_map() {
				size_t offset = this->get_offset() - 1;
				uint64_t sz;
				read(sz);
				MapPtr map = create_map();
				object_map[offset] = map;
				for (size_t i = 0; i < sz; ++i) {
					Value key = deserialize();
					Value val = deserialize();
					map_set(map, key, val);
				}
				return map;
			}
			
			Value deserialize_object_ref() {
				uint64_t offset;
				read(offset);
				auto it = object_map.find(offset);
				if (it != object_map.end()) {
					return it->second;
				}
				throw_exception_with_description("Marshal: Corrupt data - object reference.");
				return NULL;
			}
		};
		
		struct BufferRead {
			std::vector<byte>& buffer;
			size_t offset;
			BufferRead(std::vector<byte>& buffer) : buffer(buffer), offset(0) {}
			
			bool read_byte(byte& out) {
				if (offset >= buffer.size()) {
					return false;
				}
				out = buffer[offset++];
				return true;
			}
			
			size_t get_offset() { return offset; }
		};
		
		void store_value(std::vector<byte>& buffer, Value value) {
			Serializer<BufferWrite> serializer(buffer);
			serializer.serialize(value);
		}

		void write_value(int fd, Value value) {
			// TODO:
			//Serializer<FileWrite> serializer(fd);
			//serializer.serialize(value);
		}

		Value load_value(const std::vector<byte>& buffer) {
			Deserializer<BufferRead> deserializer(buffer);
			return deserializer.deserialize();
		}

		Value read_value(int fd) {
			// TODO:
			//Deserializer<FileRead> deserializer(fd);
			//return deserializer.deserialize();
			return NULL;
		}
		
		VALUE Marshal_load(const CallFrame* here, VALUE self, VALUE it) {
			std::vector<byte> buffer;
			ObjectPtr<String> str = it;
			if (str == NULL) {
				throw_exception_with_description("Marshal.load: Cannot deserialize object %@ (expected String).", it);
			}
			size_t len = string_size(str);
			buffer.reserve(len);
			char b[len];
			string_copy_to(str, b, len);
			buffer.insert(buffer.begin(), b, b+len);
			
			return load_value(buffer);
		}
		
		VALUE Marshal_store(const CallFrame* here, VALUE self, VALUE it) {
			std::vector<byte> buffer;
			store_value(buffer, it);
			PreallocatedStringData data;
			preallocate_string_data(data, buffer.size());
			std::copy(buffer.begin(), buffer.end(), data.data);
			return create_string_from_preallocated_data(data);
		}
	}
	
	namespace marshal {
		AnyObjectPtr get_module() {
			static Value* p = NULL;
			if (!p) {
				AnyObjectPtr obj = create_object(get_object_class(), 0, NULL);
				SN_OBJECT_DEFINE_METHOD(obj, "load", Marshal_load);
				SN_OBJECT_DEFINE_METHOD(obj, "store", Marshal_store);
				p = gc_create_root(obj);
			}
			return *p;
		}
	}
}

CAPI snow::VALUE snow_module_init() {
	return snow::marshal::get_module();
}