#include <memory>
#include <vector>
#include <optional>

// note: should I really support nesting?
// the idea on nesting is that it allow us to make a single big eviction decision instead of multiple small one,
// thus reducing search time.
// but this require a few thing.
// 0: the evictable value itself form a hirearchical structure
// 1: there are so much evictable value that searching form a bottleneck.
// 2: i cannot make everything more graunular (only evict at a higher level) - either because the structure is recursive with cheap atom,
// so there is no 'higher level', or i have to make choices at individual level cause the data is irregular -
// basically a map in a list take different time processing each element.
// what is like this? off the top of my head, llvm. maybe talk with John?
// i did a quick google search, and the llvm linker use 10GB of memory.
// https://www.reddit.com/r/rust/comments/azupon/compile_times_and_out_of_memory_error/
// https://gitlab.haskell.org/ghc/ghc/-/issues/13059
// https://stackoverflow.com/questions/11282305/compiler-memory-consumption-with-template-libraries-boost-eigen
// https://github.com/crystal-lang/crystal/issues/4864
// more memory stuff. seems to be a universal problem.

// this bring up a question: what is the story?
// right now this is more hammer then nail -
// i figured out DTR, and being a PList, my instinct is to generalize it to normal programming language.
// this is why i spent lots of time trying to make nesting work - because it is there, begging to be created.
// still, we have to find an application that need this.

// is 'make a language that automatically memorize and checkpoint' a selling point? will ppl truly care?
// also: what is the technical challenge to make it happend?
// also: blindly doing the two thing will simply make the program slower/consumer more space.
// we need PGO/a bunch of system optimizations tricks/possibly dynamic profiling (e.g. for some function, dynamically selecting wether or not to memorize)

// todo: a recursive access will create a temporary stack and subtract stuff from it once the stack frame is popped.
// note: T cannot be/contain a shared_ptr.
// doing so will result in incorrect estimation of savable memory.
template<typename T>
struct uncompute_node;

struct uncompute_impl_erased {
  virtual ~uncompute_impl_erased() { }
  virtual bool evictable() = 0;
  // assume evictable().
  // immutable - evict by returning the new state, and let uncompute_node change it.
  // doing this hoop because if i modify the unique_ptr directly this object will die.
  virtual void evict() = 0;
  // assume evictable().
  virtual size_t evict_cost() = 0;
 };

// how do we measure memory usage of an object?
// a simple approach is to walk the size of the tree.
// however, that will not be accurate for grap.
// in order to address sharing,
// we simply measure memory usage going into/outof every stack frame.
// so, the difference in memory size is the memory occupied.
// sadly, this do not play well with GC... future work.
// note: solved. we simply not care about uncollected garbage and evict them alongside eviction.
// i got inspired by liveness based garbage collection, but this work has 0 connection to it.
// how/should i cite those folks?
std::vector<size_t> memory_stack;
size_t current_memory;

// An RAII helper class.
// when an unique uncompute_ptr is copied, we have to subtract the size of the object inside,
// from all recursive parent.
// since this walk is O(n) per access,
// to amoritize, we use this class to lazily update the subtraction.
template<typename T>
struct get_scope {
  
};

template<typename T>
struct uncompute_impl : uncompute_impl_erased {
  // warning: parent.data is a shared_ptr possibly holding unique ownership.
  // this mean tat after modifying the data pointer, this object may die.
  // so, the mutation sould come in the very last.

  // rematerialize if needed, then return.
  virtual T get(uncompute_node<T>& parent, get_scope<T>&) = 0;
  // the object will be in evicted state afterward.
  virtual T steal(uncompute_node<T>& parent, get_scope<T>&) = 0;
};

template <typename T>
struct uncompute_node {
  std::shared_ptr<uncompute_impl<T>> data;
  std::optional<std::weak_ptr<uncompute_node<T>>> parent; // todo: fix type - use type erasure to allow for different parent type.
};

std::vector<std::weak_ptr<uncompute_impl_erased>> evictables;

template<typename T>
struct uncompute_ptr {
  std::shared_ptr<uncompute_node<T>> ptr;
};

template<typename T>
using weak_uncompute_ptr = std::weak_ptr<uncompute_node<T>>;

template<typename T>
struct atomic : uncompute_impl<T> {
  
};

template<typename T, typename... Args>
struct materialized : uncompute_impl<T> {
  
};

template<typename T, typename... Args>
struct evicted : uncompute_impl<T> {
  
};

int main() {
  return 0;
}
