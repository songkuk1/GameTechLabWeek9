template<typename... Args>
class TDelegate
{
public:
	using HandlerType = std::function<void(Args...)>;

	void Add(const HandlerType& Handler);                     
	template<typename T>
	void AddDynamic(T* Instance, void (T::* Func)(Args...));     
	void Broadcast(Args... args);

private:
	// 등록한 함수들이 여기에 저장된다
	std::vector<HandlerType> Handlers;  
	
};