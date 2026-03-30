 class Iterator {
	 std::unique_ptr<TestRange> impl;
	 public:
		 Iterator(std::unique_ptr<TestRange> p) : impl(std::move(p)) {}
		 int operator*() const {
			 return impl->deref();

		}
		Iterator& operator++() {
			impl->inc(); return *this;

		}
		bool operator!=(const Iterator& other) const {
			return !impl->equals(*other.impl);

		}

};
