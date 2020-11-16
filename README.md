# A `string_view` that remembers if it was a C string

# The Problem

When interfacing with C APIs, one often encounters functions that
take a null-terminated byte array as an argument, e.g.


    int open(const char *pathname, int flags);


From a C++ context, a C call like this is often moved into some convenience
wrapper function that provides additional type safety, for example


    fd_type open(const std::string& pathname, int flags = 0) {
      return ::open(pathname.c_str(), flags);
    }


One problem with this function is that when it is called with a string
literal as argument


    auto fd = open("/dev/null");


a temporary `std::string` object will be created that needs to copy the
string literal into an internal buffer.

In C++17, the new `std::string_view` class was introduced into the standard
to avoid this unnecessary copy. It is a non-owning view that can be
transparently used for `std::string`, `const char*` or any other form of byte
buffer.

However, in this particular example it only moves the issue to another place:
Since the `string_view` is not necessarily null-terminated, it is not possible
to simply forward the contained `const char*` to the C function. Instead, if the
view is not null-terminated, a copy needs to be created.

The catch-22 here is that it is *impossible* to safely check whether a given
string_view is null-terminated, since the byte we'd need to check may not even
be mapped to valid memory. So we're required to write code like this:


    fd_type open(std::string_view sv, int flags = 0) {
      return ::open(std::string{sv}.c_str(), flags);
    }


Of course, this is a well-known problem. To quote the original string_view
proposal from 2013:

> We could imagine inventing a more complex interface that records whether the
  input string was null-terminated, giving the user the option to use that
  string when trying to pass a string_view to a legacy or C function expecting
  a null-terminated `const char*`. This proposal doesn't include such an
  interface because it would make string_view bigger or more expensive, and
  because there's no existing practice to guide us toward the right interface.

# The Solution

When *receiving* a string_view we can not possibly tell whether it is
null-terminated or not, however when *creating* a string_view the situation is
not so bleak: When a string_view is created from a `std::string` or from a
string literal, it is guaranteed that a null byte exists after the string.

And when creating substrings or removing suffixes, at least we know that *some*
byte exists after the buffer, so at least we can check if it is null or not
without accessing invalid memory.

So all we have to do is to remember this information from the construction of
the `string_view`, and then we could implement a `is_cstring()` member function
that can be used to tell us whether we can directly use the pointer as a C
string or not:


    fd_type open(bev::string_view sv, int flags = 0) {
      if (sv.is_cstring())
        return ::open(sv.data(), flags);
      else
        return ::open(std::string{sv}.c_str(), flags);
    }


Sadly, just adding a `bool` member to capture this single bit of information
would most likely increase the size of the struct by 8 bytes in total to
preserve alignment, a 50% increase. That's less than ideal.

One common technique to avoid this is the "bit stuffing" of pointers: When a
pointer has certain alignment requirements, its bottom-most bits must be always
zero and can be repurposed to store flags. However that's not possible here,
since in the most common case the pointer will be a `const char*`, which doesnt
have any alignment requirements.

There is one other member of `string_view`, namely it's length. Could we
possibly find any unused bits in there? Why, yes, indeed. In libstdc++, this
is implemented as


    return (npos - sizeof(size_type) - sizeof(void*))
            / sizeof(value_type) / 4;


So it already is guaranteed to have at least two unused bits. In `libcxx` this
actually returns `npos`, but we only need a single bit so there should be no
problem to reduce this to `npos / 2`. Very few people work with strings larger
than 8 EiB, after all. We call this the `safederef flag` because, well, naming
is hard.

After all this exposition the actual code is embarassingly simple:


    bool string_view::is_cstring()
    {
      if (test_safederef_flag(len_))
        return str_[this->length()] != 0;
      return false;
    }

The [full list of changes][1] is slightly bigger, but it still turned out to be
far less ugly than I had anticipated.

We could also check once at construction and cache the result, but doing it
on every call also covers changes to the string after the `string_view` was
created.

So, tl;dr:

 - The highest bit of `len_` is reserved as a `safederef` flag
 - When creating a string view from a std::string or from a `const char*`, this
   is set to true.
 - When copying a string_view or when creating substring, the flag is preserved.
 - Then `is_cstring()` can be implemented as `str_[len_] == 0`.


 [1]: https://github.com/lava/string_view/commit/abcf453359c8d1ba74d5686d0f1cdfec67d67380

# The Caveat

So, can we just call it a day and add this to the standard? No.

Even though the binary layout is the same as `std::string_view` is exactly the
same (if you're adventurous you could even
`reinterpret_cast<bev::string_view*>(&std::string_view)` and everything should
work), the semantics of the `len_` field changed and passing a new string_view
to a function expecting the previous version would produce weird results.
So this would be an ABI break.

Also, I didn't actually bother to propely benchmark this, so I have no proof
that this would even result in significant performance gains.

That said, if you *did* benchmark and you see there are unnecessary string
copies going on when calling C functions, feel free to use the string_view class
in this repository as a drop-in replacement.