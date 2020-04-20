//-----------------------------------------------------------------------------
// Helpers for iteration.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_ITERATION_H
#define SOLVESPACE_ITERATION_H

#include "solvespace.h"

namespace SolveSpace {

template<typename T>
class List;
template<typename T, typename H>
class IdList;

template<typename Container>
struct GetValueTypeHelper {
    using type = typename Container::value_type;
};

template<typename T>
struct GetValueTypeHelper<List<T>> {
    using type = T;
};

template<typename T, typename H>
struct GetValueTypeHelper<IdList<T, H>> {
    using type = T;
};

template<typename Container>
using GetValueType = typename GetValueTypeHelper<Container>::type;


/// The value accessed thru a const container is the const value.
template<typename Container>
struct GetValueTypeHelper<const Container> {
    using type = typename std::add_const<GetValueType<Container>>::type;
};

// Paper over some API differences between List<>, IdList<>, and stl-shaped containers.
template<typename T>
inline int GetSize(const List<T> &container) {
    return container.n;
}

template<typename T, typename H>
inline int GetSize(const IdList<T, H> &container) {
    return container.n;
}

template<typename Container>
inline int GetSize(const Container &container) {
    return static_cast<int>(container.size());
}


/// Acts like a pointer, except internally uses a container pointer and the index of the element,
/// checking length every time.
/// They may also be used as iterators themselves, just like pointers.
template<typename ContainerType>
class IndexRef {
public:
    /// default constructor: universal past-the-end iterator.
    IndexRef() : i_(-1) {
    }

    IndexRef(const IndexRef &) = default;

    /// Construct from container and index. Negative indices are permanently past-the-end.
    IndexRef(ContainerType &container, int i = -1) : con_(&container), i_(i) {
    }

    /// In conditional expressions, true if this is a non-end iterator.
    explicit operator bool() const noexcept {
        return !IsEnd();
    }

    /// actually random access in theory, but haven't written all the right methods.
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = GetValueType<ContainerType>;
    using difference_type   = int;
    using pointer           = value_type *;
    using reference         = value_type &;

    /// pre-increment (only affects non-negative indices)
    IndexRef &operator++() noexcept {
        if(i_ >= 0) {
            ++i_;
            // fprintf(stderr, "index: %d   size: %d\n", GetIndex(), GetSize(*con_));
        }

        return *this;
    }

    /// post-increment (only affects non-negative indices)
    IndexRef operator++(int) noexcept {
        IndexRef ret{*this};
        ++(*this);
        return ret;
    }

    /// pre-decrement (only affects non-negative indices)
    IndexRef &operator--() noexcept {
        if(i_ >= 0) {
            --i_;
        }
        return *this;
    }

    /// post-decrement (only affects non-negative indices)
    IndexRef operator--(int) noexcept {
        IndexRef ret{*this};
        --(*this);
        return ret;
    }

    /// dereference to get an element reference.
    reference operator*() const {
        if(IsEnd()) {
            ssassert(!IsEnd(), "Cannot dereference an invalid index");
        }
        return (*con_)[i_];
    }

    /// arrow operator, to get an element pointer which in turn has the arrow operator applied.
    /// That is, this works just the way it should.
    pointer operator->() const {
        if(IsEnd()) {
            ssassert(!IsEnd(), "Cannot dereference an invalid index");
            return nullptr;
        }
        return &((*con_)[i_]);
    }

    /// Equality comparison operator.
    //
    /// Per the requirements of the iterator concepts, all past-the-end iterators compare equal,
    /// even though there is internally a difference between a negative index (permanently
    /// past-the-end) and an index that is too large (could return to valid usage if underlying size
    /// changes or the iterator is decremented)
    bool operator==(IndexRef const &other) const {
        if(IsEnd() && other.IsEnd()) {
            return true;
        }
        if(IsEnd() || other.IsEnd()) {
            return false;
        }
        ssassert(con_ == other.con_, "Mixing iterators from different containers is not allowed");
        return (con_ == other.con_) && (i_ == other.i_);
    }

    /// Inequality comparison operator.
    /// See operator== for caveat.
    bool operator!=(IndexRef const &other) const {
        return !(*this == other);
    }

    /// Gets a pointer to the value, or nullptr if this is past-the-end
    value_type *Get() const noexcept {
        return IsEnd() ? nullptr : &((*con_)[i_]);
    }

    /// Gets the numeric index in the container, or -1 for any past-the-end iterator.
    int GetIndex() const noexcept {
        return IsEnd() ? -1 : i_;
    }

    /// True if this is any form of "past-the-end" iterator
    bool IsEnd() const noexcept {
        return i_ < 0 || con_ == nullptr || i_ >= GetSize(*con_);
    }

    /// Difference operator - returns the distance in indices between two of these iterators.
    /// Maintains the facade that all past-the-end iterators are equal.
    int operator-(const IndexRef &rhs) const {
        if(IsEnd() && rhs.IsEnd()) {
            return 0;
        }
        if(con_ == nullptr && rhs.con_ != nullptr) {
            return GetSize(*rhs.con_) - rhs.i_;
        }

        if(rhs.con_ == nullptr && con_ != nullptr) {
            return i_ - GetSize(*con_);
        }
        ssassert(con_ == rhs.con_, "Can't subtract between containers");
        return i_ - rhs.i_;
    }

private:
    ContainerType *con_;
    int i_;
};


/// Index iterator: the "elements" iterated over are actually IndexRef, rather than direct access to
/// elements themselves. This means your range-for gets pointer-like indices which don't get
/// invalidated if the backing store of the container is reallocated, etc.
//
/// Most behavior follows from the fact this is basically a "wrapper" around an IndexRef<> iterator.
template<typename ContainerType>
class IndexIterator {
public:
    /// default constructor: universal past-the-end iterator.
    IndexIterator()                      = default;
    IndexIterator(const IndexIterator &) = default;
    /// Construct from container and index. Negative indices are permanently past-the-end.
    IndexIterator(ContainerType &container, int i = -1) : inner_(container, i) {
    }
    explicit operator bool() const noexcept {
        return !IsEnd();
    }

    using value_type        = GetValueType<ContainerType>;
    using iterator_category = std::input_iterator_tag;
    using difference_type   = int;
    using pointer           = const IndexRef<ContainerType> &;
    using reference         = const IndexRef<ContainerType> &;

    /// pre-increment (only affects non-negative indices)
    IndexIterator &operator++() noexcept {
        ++inner_;
        return *this;
    }

    /// post-increment (only affects non-negative indices)
    IndexIterator operator++(int) noexcept {
        IndexIterator ret{*this};
        ++inner_;
        return ret;
    }

    /// pre-decrement (only affects non-negative indices)
    IndexIterator &operator--() noexcept {
        --inner_;
        return *this;
    }

    /// post-decrement (only affects non-negative indices)
    IndexIterator operator--(int) noexcept {
        IndexIterator ret{*this};
        --inner_;
        return ret;
    }

    /// dereference to get an IndexRef, which can itself be used to get a reference to an element.
    reference operator*() const {
        ssassert(!IsEnd(), "Cannot dereference an invalid index");
        return inner_;
    }

    /// arrow operator to get an IndexRef, which can itself be used to get a reference to an
    /// element.
    pointer operator->() const {
        ssassert(!IsEnd(), "Cannot dereference an invalid index");
        return inner_;
    }

    /// Equality comparison operator.
    //
    /// Per the requirements of the iterator concepts, all past-the-end iterators compare equal,
    /// even though there is internally a difference between a negative index (permanently
    /// past-the-end) and an index that is too large (could return to valid usage if underlying size
    /// changes or the iterator is decremented)
    bool operator==(IndexIterator const &other) const {
        return inner_ == other.inner_;
    }

    /// Inequality comparison operator.
    /// See operator== for caveat.
    bool operator!=(IndexIterator const &other) const {
        return !(*this == other);
    }

    /// True if this is any form of "past-the-end" iterator
    bool IsEnd() const {
        return inner_.IsEnd();
    }

private:
    IndexRef<ContainerType> inner_;
};

/// Looks enough like a container to let a range-for iterate using it.
/// Construct with two iterators (or pointers!) for begin() and end(), where,
/// as is convention, end() is the past-the-end iterator.
template<typename IteratorType>
class RangeIterationProxy {
public:
    RangeIterationProxy(IteratorType beginIter, IteratorType endIter)
        : begin_(beginIter), end_(endIter) {
    }

    IteratorType begin() const {
        return begin_;
    }
    IteratorType end() const {
        return end_;
    }

private:
    IteratorType begin_;
    IteratorType end_;
};

/// RangeIterationProxy helper function for deducing type: pass two iterators to get something that
/// works in range-for.
template<typename IteratorType>
inline RangeIterationProxy<IteratorType> rangeFromBounds(const IteratorType &beginIter,
                                                         const IteratorType &endIter) {
    return {beginIter, endIter};
}

/// Iterate over a container via its indices: each pass of the range-for gets an IndexRef<> which is
/// converted to the correct pointer at time of use, meaning it won't get invalidated just because
/// the backing array got reallocated. The end iterator is also dynamic, in that it's not "iterate
/// all elements present at the start of the loop", but "iterate until there are no more valid
/// indices we haven't iterated over" making it OK to insert (and sometimes even remove, from
/// indices greater than the current one) items from the container being iterated.
template<typename T>
inline RangeIterationProxy<IndexIterator<T>> indices(T &container, int start = 0) {
    return {IndexIterator<T>{container, start}, IndexIterator<T>{container}};
}


} // namespace SolveSpace

#endif
