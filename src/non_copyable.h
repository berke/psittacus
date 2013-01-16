class non_copyable
{
protected:
	non_copyable() { }
	~non_copyable() { }
private:
	non_copyable(const non_copyable&);
	non_copyable &operator=(const non_copyable&);
};
