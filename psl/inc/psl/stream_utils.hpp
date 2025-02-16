#pragma once
#include "ustream.hpp"
#include "ustring.hpp"
#include <iomanip>

namespace utility::string {
/// \brief transforms the input value to a psl::string8_t containing the hex characters.
/// \param[in] value the value to transform
/// \returns a string containing the hex value.
template <typename T>
inline psl::string8_t to_hex(const T& value) {
	psl::string8::stream stream;
	stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << value;
	return stream.str();
}

/*template<typename T>
inline size_t SizeofTypeInHex()
{
return std::setw(sizeof(T) * 2) + 2;
}*/

template <typename TOut, typename TIn>
inline TOut lexical_cast(const TIn& in) {
	// though this needs the 0x prefix so it knows it is hex
	// unsigned int x = lexical_cast<unsigned int>("0xdeadbeef");
	TOut out;
	psl::string8::stream ss;
	ss << in;
	ss >> out;
	return out;
}

/// \brief cast the input string containing a hex value, to the given type
/// \tparam T the type to convert the output to.
/// \param[in] str the string containing the hex value.
/// \returns an object of type T that has been constructed using the hex value.
template <typename T>
inline T from_hex(const psl::string8_t& str) {
	return lexical_cast<T>(str);
}

/// \brief cast the input string containing a hex value, to the given type
/// \tparam T the type to convert the output to.
/// \param[in] str the string containing the hex value.
/// \returns an object of type T that has been constructed using the hex value.
template <typename T>
inline T from_hex(const psl::string16_t& str) {
	return lexical_cast<T>(str);
}

/// \brief cast the input string containing a hex value, to the given type
/// \tparam T the type to convert the output to.
/// \param[in] str the string containing the hex value.
/// \returns an object of type T that has been constructed using the hex value.
template <typename T>
inline T from_hex(const psl::string32_t& str) {
	return lexical_cast<T>(str);
}
}	 // namespace utility::string
