#include <memory>
#include <vector>
#include <optional>
#include <cassert>

// in dtr, every node has to keep track of its parent and children.
// will this become a problem?
// if we support nesting of call tree, maybe this will be harder.

// should we allow multiple eviction function to a single object?
// in this case, evicting a higher function is equivalent to evicting multiple function at once.
// how should it be coded?
// whenever something is evicted, the path of its and its parent is connected undirectionaly.
// all connected path belong in a single equivalent class.

// todo: lets code it without nesting support for now.
// lateron when we done normal dtr we can add it back -
// the code for nesting is much more complex, so it make sense to do in stage.

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

// is 'make a language that automatically memorize and checkpoint' a selling point? will ppl truly care?
// also: what is the technical challenge to make it happend?
// also: blindly doing the two thing will simply make the program slower/consumer more space.
// we need PGO/a bunch of system optimizations tricks/possibly dynamic profiling (e.g. for some function, dynamically selecting wether or not to memorize)

// todo: a recursive access will create a temporary stack and subtract stuff from it once the stack frame is popped.
// note: T cannot be/contain a shared_ptr.
// doing so will result in incorrect estimation of savable memory.
template<typename T>
struct uncompute_node;

struct uncompute_impl_base {
  virtual ~uncompute_impl_base() { }
  virtual bool evictable() = 0;
  // assume evictable().
  // immutable - evict by returning the new state, and let uncompute_node change it.
  // doing this hoop because if i modify the unique_ptr directly this object will die.
  virtual void evict() = 0;
  // assume evictable().
  // make it unevictable().
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

// a token do raii to lock/unlock value.
// additionally, it also update the memory after usage.
template<typename T>
struct token {
  const T& t;

};

template<typename T>
struct uncompute_impl : uncompute_impl_base {
  // warning: parent.data is a shared_ptr possibly holding unique ownership.
  // this mean tat after modifying the data pointer, this object may die.
  // so, the mutation sould come in the very last.

  // rematerialize if needed, then return.
  virtual T get(uncompute_node<T>& parent) = 0;
  // the object will be in evicted state afterward.
  virtual T steal(uncompute_node<T>& parent) = 0;
};

struct uncompute_node_base {
  // the core of our approach rely on
  // estimating how much memory can be free when we evict an object.
  // in a more formal manner, the memory of an object is the memory of all object
  // dominated by it in the object graph.
  size_t memory;
  // there are 4 cases where memory change.
  // 0: de-uniquification - in this case we subtract the total memory
  // 1: rematerialization
  // 2: eviction - the hardest of the three, as we can not amoritize update in O(1).
  // we update the current node without regard of its parent.
  // to make this more accurate, we can keep a bank of time,
  // and forever non-propagated memory change, the bank increase.
  // everytime we propagate change, the bank decrease.
  // we can additionally only propagate change when memory_delta is greater then a portion of memory -
  // when it is pretty accurate, dont bother updating.
  // but still, even without those measure, seems like eviction wont be a game-breaking harm:
  // miscalculating memory mean it may evict an function up the call tree,
  // that include the evicted subobject.
  // but for this to happend, that eviction basically undo all the subeviction,
  // and for that eviction to be profitable that eviction must be profitable without the subeviction.
  // In anotherword: stale memory estimation due to eviction only force one to evict at a higher level,
  // where the eviction is still profitable.
  // 3: reuniqification:
  // if we were to handle this, for every object we must keep track of all its parent.
  // the cost sounds far too great, so we simply do not handle it.
  // like eviction memory update, not handling memory update force higher level eviction.
  size_t memory_delta = 0;
  virtual ~uncompute_node_base() { } // do we need this?
  // maybe nullptr if does not have a single unique parent.
  // even if a unique parent exist, may still be null,
  // because we cant recover once parent become null.
  // so, memory will underestimate.
  // look a bit inevitable with the current approach.
  std::weak_ptr<uncompute_node_base> parent;
  void stage(size_t delta) {
    memory_delta += delta;
  }
  void commit() {
    if (memory_delta != 0) {
      assert(memory >= memory_delta);
      memory -= memory_delta;
      if (auto strong_parent = parent.lock()) {
        strong_parent->stage(memory_delta);
      }
      memory_delta = 0;
    }
  }
  void deunique() {
    commit();
    if (auto strong_parent = parent.lock()) {
      strong_parent->stage(memory);
    }
    parent.reset();
  }
  uncompute_node_base() = delete;
  explicit uncompute_node_base(size_t memory) : memory(memory) { }
  explicit uncompute_node_base(size_t memory, std::weak_ptr<uncompute_node_base> parent) : memory(memory), parent(parent) { }
};

template <typename T>
struct uncompute_node : uncompute_node_base {
  std::shared_ptr<uncompute_impl<T>> data;
};

std::vector<std::weak_ptr<uncompute_impl_base>> evictables;

template<typename T>
struct uncompute_ptr {
  std::shared_ptr<uncompute_node<T>> ptr;
  uncompute_ptr(const uncompute_ptr<T>& un) : ptr(un.ptr) {
    ptr->deunique();
  }
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
