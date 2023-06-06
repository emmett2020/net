// #ifndef NET_SRC_TYPE_TRAITS_HPP_
// #define NET_SRC_TYPE_TRAITS_HPP_

// #include <type_traits>

// namespace net {
// using std::add_const;
// using std::add_lvalue_reference;
// using std::alignment_of;
// using std::conditional;
// using std::decay;
// using std::declval;
// using std::enable_if;
// using std::false_type;
// using std::integral_constant;
// using std::is_base_of;
// using std::is_class;
// using std::is_const;
// using std::is_constructible;
// using std::is_convertible;
// using std::is_copy_constructible;
// using std::is_destructible;
// using std::is_function;
// using std::is_move_constructible;
// using std::is_nothrow_copy_constructible;
// using std::is_nothrow_destructible;
// using std::is_object;
// using std::is_pointer;
// using std::is_reference;
// using std::is_same;
// using std::is_scalar;
// using std::remove_cv;
// using std::remove_pointer;
// using std::remove_reference;
// using std::true_type;

// template <bool Condition, typename Type = int>
// struct constraint : enable_if<Condition, Type> {};

// template <typename T>
// struct remove_cvref : remove_cv<typename std::remove_reference<T>::type> {};

// struct defaulted_constraint {
//   constexpr defaulted_constraint() {}
// };

// template <typename...>
// struct conjunction : true_type {};
// template <typename T>
// struct conjunction<T> : T {};
// template <typename Head, typename... Tail>
// struct conjunction<Head, Tail...> : conditional<Head::value, conjunction<Tail...>, Head>::type
// {};

// template <typename>
// struct void_type {
//   typedef void type;
// };

// template <typename>
// struct result_of;

// template <typename F, typename... Args>
// struct result_of<F(Args...)> : std::invoke_result<F, Args...> {};
// }  // namespace net
// #endif  // NET_SRC_TYPE_TRAITS_HPP_