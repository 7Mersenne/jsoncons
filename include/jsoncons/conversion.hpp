// Copyright 2017 Daniel Parker
// Distributed under the Boost license, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// See https://github.com/danielaparker/jsoncons for latest version

#ifndef JSONCONS_CONVERSION_HPP
#define JSONCONS_CONVERSION_HPP

#include <iostream>
#include <string>
#include <tuple>
#include <array>
#include <type_traits>
#include <memory>
#include <jsoncons/json_content_handler.hpp>
#include <jsoncons/json_decoder.hpp>
#include <jsoncons/basic_json.hpp>
#include <jsoncons/json_serializing_options.hpp>
#include <jsoncons/json_serializer.hpp>
#include <jsoncons/jsoncons_utilities.hpp>
#include <jsoncons/json_type_traits.hpp>
#include <jsoncons/staj_reader.hpp>

namespace jsoncons {

// conversion_traits

template <class T, class Enable = void>
struct conversion_traits
{
    template <class CharT>
    static T decode(basic_staj_reader<CharT>& reader);

    template <class CharT>
    static T decode(basic_staj_reader<CharT>& reader, std::error_code& ec);

    template <class CharT>
    static void encode(const T& val, basic_json_content_handler<CharT>& serializer);
};
// conversion_traits specializations

// vector like

template <class T>
struct conversion_traits<T,
    typename std::enable_if<detail::is_vector_like<T>::value
>::type>
{
    //typedef typename std::iterator_traits<typename T::iterator>::value_type value_type;
    typedef typename T::value_type value_type;

    template <class CharT>
    static T decode(basic_staj_reader<CharT>& reader);

    template <class CharT>
    static T decode(basic_staj_reader<CharT>& reader, std::error_code& ec);

    template <class CharT>
    static void encode(const T& val, basic_json_content_handler<CharT>& serializer);
};
// std::array

template <class T, size_t N>
struct conversion_traits<std::array<T,N>>
{
    typedef typename std::array<T,N>::value_type value_type;

    template <class CharT>
    static std::array<T, N> decode(basic_staj_reader<CharT>& reader);

    template <class CharT>
    static std::array<T, N> decode(basic_staj_reader<CharT>& reader, std::error_code& ec);

    template <class CharT>
    static void encode(const std::array<T, N>& val, basic_json_content_handler<CharT>& serializer);
};

// map like

template <class T>
struct conversion_traits<T,
    typename std::enable_if<detail::is_map_like<T>::value
>::type>
{
    typedef typename T::mapped_type mapped_type;
    typedef typename T::value_type value_type;
    typedef typename T::key_type key_type;

    template <class CharT>
    static T decode(basic_staj_reader<CharT>& reader);

    template <class CharT>
    static T decode(basic_staj_reader<CharT>& reader, std::error_code& ec);

    template <class CharT>
    static void encode(const T& val, basic_json_content_handler<CharT>& serializer);
};
// std::tuple

namespace detail { namespace streaming {

template<size_t Pos, class Tuple>
struct tuple_helper
{
    using element_type = typename std::tuple_element<std::tuple_size<Tuple>::value - Pos, Tuple>::type;
    using next = tuple_helper<Pos - 1, Tuple>;
    
    template <class CharT>
    static void encode(const Tuple& tuple, basic_json_content_handler<CharT>& handler)
    {
        conversion_traits<element_type>::encode(std::get<std::tuple_size<Tuple>::value - Pos>(tuple),handler);
        next::encode(tuple, handler);
    }
};

template<class Tuple>
struct tuple_helper<0, Tuple>
{
    template <class CharT>
    static void encode(const Tuple&, basic_json_content_handler<CharT>&)
    {
    }
};

}}

template<typename... E>
struct conversion_traits<std::tuple<E...>>
{
private:
    using helper = detail::streaming::tuple_helper<sizeof...(E), std::tuple<E...>>;
public:

    template <class CharT>
    static std::tuple<E...> decode(basic_staj_reader<CharT>& reader);

    template <class CharT>
    static std::tuple<E...> decode(basic_staj_reader<CharT>& reader, std::error_code& ec);

    template <class CharT>
    static void encode(const std::tuple<E...>& val, basic_json_content_handler<CharT>& serializer);
};

}

#include <jsoncons/json_stream_reader.hpp>
#include <jsoncons/staj_iterator.hpp>

namespace jsoncons {

// Implementation

template <class T, class Enable>
template <class CharT>
static T conversion_traits<T,Enable>::decode(basic_staj_reader<CharT>& reader)
{
    json_decoder<basic_json<CharT>> decoder;
    reader.accept(decoder);
    return decoder.get_result().template as<T>();
}

template <class T, class Enable>
template <class CharT>
static T conversion_traits<T,Enable>::decode(basic_staj_reader<CharT>& reader,
                                               std::error_code& ec)
{
    json_decoder<basic_json<CharT>> decoder;
    reader.accept(decoder, ec);
    return decoder.get_result().template as<T>();
}

template <class T, class Enable>
template <class CharT>
static void conversion_traits<T,Enable>::encode(const T& val, basic_json_content_handler<CharT>& serializer)
{
    auto j = json_type_traits<basic_json<CharT>, T>::to_json(val);
    j.dump(serializer);
}

// conversion_traits specializations

// vector like

template <class T>
template <class CharT>
static T conversion_traits<T,typename std::enable_if<detail::is_vector_like<T>::value>::type>::decode(basic_staj_reader<CharT>& reader)
{
    T v;
    basic_staj_array_iterator<CharT,value_type> it(reader);

    for (const auto& item : it)
    {
        v.push_back(item);
    }
    return v;
}

template <class T>
template <class CharT>
static T conversion_traits<T,typename std::enable_if<detail::is_vector_like<T>::value>::type>::decode(basic_staj_reader<CharT>& reader,
                                                                                                        std::error_code& ec)
{
    T v;
    basic_staj_array_iterator<CharT,value_type> end;
    basic_staj_array_iterator<CharT,value_type> it(reader, ec);

    while (it != end && !ec)
    {
        v.push_back(*it);
        it->increment(ec);
    }
    return v;
}

template <class T>
template <class CharT>
static void conversion_traits<T,typename std::enable_if<detail::is_vector_like<T>::value>::type>::encode(const T& val, basic_json_content_handler<CharT>& serializer)
{
    serializer.begin_array();
    for (auto it = std::begin(val); it != std::end(val); ++it)
    {
        conversion_traits<value_type>::encode(*it,serializer);
    }
    serializer.end_array();
    serializer.flush();
}

// std::array

template <class T, size_t N>
template <class CharT>
static std::array<T, N> conversion_traits<std::array<T,N>>::decode(basic_staj_reader<CharT>& reader)
{
    std::array<T,N> v;
    basic_staj_array_iterator<CharT,value_type> it(reader);

    size_t i = 0;
    for (const auto& item : it)
    {
        if (i < N)
        {
            v[i++] = item;
        }
    }
    return v;
}

template <class T, size_t N>
template <class CharT>
static std::array<T, N> conversion_traits<std::array<T,N>>::decode(basic_staj_reader<CharT>& reader,
                                                                     std::error_code& ec)
{
    std::array<T,N> v;
    basic_staj_array_iterator<CharT,value_type> end;
    basic_staj_array_iterator<CharT,value_type> it(reader, ec);

    for (size_t i = 0; it != end && i < N && !ec; ++i)
    {
        v[i] = *it;
        it->increment(ec);
    }
    return v;
}

template <class T, size_t N>
template <class CharT>
static void conversion_traits<std::array<T,N>>::encode(const std::array<T, N>& val, basic_json_content_handler<CharT>& serializer)
{
    serializer.begin_array();
    for (auto it = std::begin(val); it != std::end(val); ++it)
    {
        conversion_traits<value_type>::encode(*it,serializer);
    }
    serializer.end_array();
    serializer.flush();
}



// map like

template <class T>
template <class CharT>
static T conversion_traits<T,typename std::enable_if<detail::is_map_like<T>::value>::type>::decode(basic_staj_reader<CharT>& reader)
{
    T m;
    basic_staj_object_iterator<CharT,mapped_type> it(reader);

    for (const auto& kv : it)
    {
        m.emplace(kv.first,kv.second);
    }
    return m;
}

template <class T>
template <class CharT>
static T conversion_traits<T,typename std::enable_if<detail::is_map_like<T>::value>::type>::decode(basic_staj_reader<CharT>& reader,
                                                                                                     std::error_code& ec)
{
    T m;
    basic_staj_array_iterator<CharT,value_type> end;
    basic_staj_array_iterator<CharT,value_type> it(reader, ec);

    while (it != end && !ec)
    {
        m.emplace(it->first,it->second);
        it->increment(ec);
    }
    return m;
}

template <class T>
template <class CharT>
static void conversion_traits<T,typename std::enable_if<detail::is_map_like<T>::value>::type>::encode(const T& val, basic_json_content_handler<CharT>& serializer)
{
    serializer.begin_object();
    for (auto it = std::begin(val); it != std::end(val); ++it)
    {
        serializer.name(it->first);
        conversion_traits<mapped_type>::encode(it->second,serializer);
    }
    serializer.end_object();
    serializer.flush();
}

template<typename... E>
template <class CharT>
static std::tuple<E...> conversion_traits<std::tuple<E...>>::decode(basic_staj_reader<CharT>& reader)
{
    json_decoder<basic_json<CharT>> decoder;
    reader.accept(decoder);
    return decoder.get_result().template as<std::tuple<E...>>();
}

template<typename... E>
template <class CharT>
static std::tuple<E...> conversion_traits<std::tuple<E...>>::decode(basic_staj_reader<CharT>& reader,
                                                                      std::error_code& ec)
{
    json_decoder<basic_json<CharT>> decoder;
    reader.accept(decoder, ec);
    return decoder.get_result().template as<std::tuple<E...>>();
}

template<typename... E>
template <class CharT>
static void conversion_traits<std::tuple<E...>>::encode(const std::tuple<E...>& val, basic_json_content_handler<CharT>& serializer)
{
    serializer.begin_array();
    helper::encode(val, serializer);
    serializer.end_array();
}

// decode_json

template <class T, class CharT>
T decode_json(const std::basic_string<CharT>& s)
{
    std::basic_istringstream<CharT> is(s);
    basic_json_stream_reader<CharT> reader(is);
    return conversion_traits<T>::decode(reader);
}

template <class T, class CharT>
T decode_json(const std::basic_string<CharT>& s,
              const basic_json_serializing_options<CharT>& options)
{
    std::basic_istringstream<CharT> is(s);
    basic_json_stream_reader<CharT> reader(is, options);
    return conversion_traits<T>::decode(reader);
}

template <class T, class CharT>
T decode_json(std::basic_istringstream<CharT>& is)
{
    basic_json_stream_reader<CharT> reader(is);
    return conversion_traits<T>::decode(reader);
}

template <class T, class CharT>
T decode_json(std::basic_istringstream<CharT>& is,
              const basic_json_serializing_options<CharT>& options)
{
    basic_json_stream_reader<CharT> reader(is, options);
    return conversion_traits<T>::decode(reader);
}

// encode_json

template <class T, class CharT>
void encode_json(const T& val, basic_json_content_handler<CharT>& handler)
{
    conversion_traits<T>::encode(val,handler);
    handler.flush();
}

#if !defined(JSONCONS_NO_DEPRECATED)
template <class T, class CharT>
void encode_fragment(const T& val, basic_json_content_handler<CharT>& handler)
{
    conversion_traits<T>::encode(val,handler);
}
#endif

template <class T, class CharT>
void encode_json(const T& val, std::basic_ostream<CharT>& os)
{
    basic_json_serializer<CharT> serializer(os);
    encode_json(val, serializer);
}

template <class T, class CharT>
void encode_json(const T& val, const basic_json_serializing_options<CharT>& options,
          std::basic_ostream<CharT>& os)
{
    basic_json_serializer<CharT> serializer(os, options);
    encode_json(val, serializer);
}

template <class T, class CharT>
void encode_json(const T& val, std::basic_ostream<CharT>& os, indenting line_indent)
{
    basic_json_serializer<CharT> serializer(os, line_indent);
    encode_json(val, serializer);
}

template <class T, class CharT>
void encode_json(const T& val, const basic_json_serializing_options<CharT>& options,
          std::basic_ostream<CharT>& os, indenting line_indent)
{
    basic_json_serializer<CharT> serializer(os, options, line_indent);
    encode_json(val, serializer);
}

template <class T, class CharT>
void encode_json(const T& val, std::basic_string<CharT>& s)
{
    basic_json_serializer<CharT,detail::string_writer<std::basic_string<CharT>>> serializer(s);
    encode_json(val, serializer);
}

template <class T, class CharT>
void encode_json(const T& val, const basic_json_serializing_options<CharT>& options,
          std::basic_string<CharT>& s)
{
    basic_json_serializer<CharT,detail::string_writer<std::basic_string<CharT>>> serializer(s, options);
    encode_json(val, serializer);
}

template <class T, class CharT>
void encode_json(const T& val, std::basic_string<CharT>& s, indenting line_indent)
{
    basic_json_serializer<CharT,detail::string_writer<std::basic_string<CharT>>> serializer(s, line_indent);
    encode_json(val, serializer);
}

template <class T, class CharT>
void encode_json(const T& val, const basic_json_serializing_options<CharT>& options,
          std::basic_string<CharT,detail::string_writer<std::basic_string<CharT>>>& s, indenting line_indent)
{
    basic_json_serializer<CharT> serializer(s, options, line_indent);
    encode_json(val, serializer);
}

}

#endif

