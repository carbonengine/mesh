//#include "../include/cmf/cmf.h"
//#include <utility>
//
//struct AnyType
//{
//	AnyType()
//	{
//	}
//	AnyType( int )
//	{
//	}
//	template <typename T>
//	constexpr operator T() const
//	{
//		return {};
//	}
//};
//
//
//template <typename T>
//constexpr size_t MemberCount()
//{
//	T t{};
//	size_t count = 0;
//	t.EnumerateMembers( [&count]( auto&&, auto&&, auto ) {
//		++count;
//	} );
//	return count;
//}
//
//namespace details {
//  template<class, class, class...>
//  struct can_invoke:std::false_type{};
//  template<class F, class...Args>
//  struct can_invoke<
//    F,
//    std::void_t<std::invoke_result_t<F( Args... )>>,
//    Args...
//  >:
//    std::true_type
//  {};
//}
//template<class F, class...Args>
//using can_invoke=details::can_invoke<F,void,Args...>;
//
//template <size_t I>
//struct MakeAnyType
//{
//	using type = AnyType;
//};
//
//template <typename T, typename... Args>
//int InvokeConstructor( Args... args )
//{
//	T{ args... };
//	return 0;
//}
//
//
//template <typename T, typename... Args>
//void TTT2()
//{
//	using g = std::invoke_result_t<InvokeConstructor( Args... )>;
//	//static_assert(can_invoke<T, Args...>::value);
//	T{ Args{}... };
//}
//
//template <typename T, size_t... I>
//T TTT( std::index_sequence<I...> int_seq )
//{
//	TTT2<T, MakeAnyType<I>::type...>();
//	//std::invoke_result_t<T && ( Args... )>
//	//static_assert( can_invoke<T, MakeAnyType<I>::type...>::value, "Yo" );
//	return T{ MakeAnyType<I>::type{}... };
//	//return T{ AnyType<I>::type... };
//}
//
//void Test()
//{
//
//	constexpr size_t v = MemberCount<cmf::Transform>();
//	TTT<cmf::Transform>( std::make_index_sequence<v>{} );
//	//TTT2<cmf::Skeleton, MakeAnyTypePack<v>::type>();
//	//static_assert(  == 5 );
//}