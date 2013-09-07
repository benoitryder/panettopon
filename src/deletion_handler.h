#ifndef DELETION_HANDLER_H_
#define DELETION_HANDLER_H_


/** @brief Handler to postpone object deletion.
 *
 * Posting deletion allows to destroy the object after its (canceled) handlers.
 */
template <typename T> class deletion_handler
{
 public:
  typedef std::unique_ptr<T> ptr_type;

  explicit deletion_handler(ptr_type ptr): ptr_(std::move(ptr)) {}
  explicit deletion_handler(T* ptr): ptr_(ptr) {}
  const deletion_handler& operator=(const deletion_handler& o)
  {
    ptr_ = o.ptr_;
  }
  deletion_handler(const deletion_handler& o)
  {
    ptr_ = std::move(o.ptr_);
  }

  void operator()()
  {
    ptr_.reset();
  }

 private:
  mutable ptr_type ptr_;
};


#endif
