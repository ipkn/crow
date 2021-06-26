#ifndef STATIC_REFLECT_H_
#define STATIC_REFLECT_H_
#include <tuple>
#include <type_traits>
#include <memory>
namespace nlohmann {
  template <typename Fn,typename Tuple,std::size_t... I>
  inline constexpr void ForEachTuple(Tuple&& tuple,
									 Fn&& fn,
									 std::index_sequence<I...>) {
	using Expander=int[];
	(void)Expander {
	  0,((void)fn(std::get<I>(std::forward<Tuple>(tuple))),0)...
	};
  }

  template <typename Fn,typename Tuple>
  inline constexpr void ForEachTuple(Tuple&& tuple,Fn&& fn) {
	ForEachTuple(
	  std::forward<Tuple>(tuple),std::forward<Fn>(fn),
	  std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>{});
  }

  template <typename T>
  struct is_field_pointer : std::false_type {};

  template <typename C,typename T>
  struct is_field_pointer<T C::*> : std::true_type {};

  template <typename T>
  constexpr auto is_field_pointer_v=is_field_pointer<T>::value;

}  // namespace nlohmann
template <typename T>
inline constexpr auto StructSchema() {
  return std::make_tuple();
}
#define SYMBOL_(Struct, ...)        \
  template <>                                    \
  inline constexpr auto StructSchema<Struct>() { \
    using _Struct = Struct;                      \
    return std::make_tuple(__VA_ARGS__);         \
  }
#define AS_(StructField) \
  std::make_tuple(&_Struct::StructField, #StructField)
#define AS__(StructField, FieldName) \
  std::make_tuple(&_Struct::StructField, FieldName)

template <typename T,typename Fn>
inline constexpr void ForEachField(T&& value,Fn&& fn) {
  constexpr auto struct_schema=StructSchema<std::decay_t<T>>();
  static_assert(std::tuple_size<decltype(struct_schema)>::value!=0,
				"StructSchema<T>() for type T should be specialized to return "
				"FieldSchema tuples, like ((&T::field, field_name), ...)");
  nlohmann::ForEachTuple(struct_schema,[&value,&fn](auto&& field_schema) {
	using FieldSchema=std::decay_t<decltype(field_schema)>;
	static_assert(
	  std::tuple_size<FieldSchema>::value>=2&&
	  nlohmann::is_field_pointer_v<std::tuple_element_t<0,FieldSchema>>,
	  "FieldSchema tuple should be (&T::field, field_name)");
	fn(value.*(std::get<0>(std::forward<decltype(field_schema)>(field_schema))),
	   std::get<1>(std::forward<decltype(field_schema)>(field_schema)));
  });
}
namespace {
  template <typename T>
  struct is_optional : std::false_type {};
  template <typename T>
  struct is_optional<std::unique_ptr<T>> : std::true_type {};
  template <typename T>
  constexpr bool isOptionalV=is_optional<std::decay_t<T>>::value;
  template <typename T>
  constexpr bool hasSchema=std::tuple_size<decltype(StructSchema<T>())>::value;
}
namespace nlohmann {
  template <typename T>
  struct adl_serializer<std::unique_ptr<T>> {
	static void to_json(json& j,const std::unique_ptr<T>& opt) {
	  j=opt?json(*opt):json(nullptr);
	}
	static void from_json(const json& j,std::unique_ptr<T>& opt) {
	  opt=!j.is_null()?std::make_unique<T>(j.get<T>()):nullptr;
	}
  };
  template <typename T>
  struct adl_serializer<T,std::enable_if_t<::hasSchema<T>>> {
	template <typename BasicJsonType>
	static void to_json(BasicJsonType& j,const T& value) {
	  ForEachField(value,[&j](auto&& field,auto&& name) { j[name]=field; });
	}
	template <typename BasicJsonType>
	static void from_json(const BasicJsonType& j,T& value) {
	  ForEachField(value,[&j](auto&& field,auto&& name) {
		if (::isOptionalV<decltype(field)>&&j.find(name)==j.end())return;
		try { j.at(name).get_to(field); } 
		catch (const std::exception&) { return; }
	  });
	}
  };
}
#endif  // STATIC_REFLECT_H_