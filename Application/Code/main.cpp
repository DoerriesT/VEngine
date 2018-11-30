#include <cstdlib>
#include <Engine.h>
#include <IGameLogic.h>
#include <ECS/EntityManager.h>
#include <ECS/Component/Component.h>
#include <iostream>


struct Component0 : public VEngine::Component<Component0>
{

};

struct Component1 : public VEngine::Component<Component1>
{

};

struct Component2 : public VEngine::Component<Component2>
{

};


class DummyLogic : public VEngine::IGameLogic
{
public:
	void init() {};
	void input(double time, double timeDelta) {};
	void update(double time, double timeDelta) {};
	void render() {};
};

template <typename T>
struct identity
{
	typedef T type;
};

template <typename... T>
void func(typename identity<std::function<void(T...)>>::type f) {
	f(3, 6, 8);
}

int main()
{
	VEngine::EntityManager entityManager;
	const VEngine::Entity *entities[32];

	for (size_t i = 0; i < 32; ++i)
	{
		entities[i] = entityManager.createEntity();

		entityManager.addComponent<Component0>(entities[i]);

		if (i % 2 == 0)
		{
			entityManager.addComponent<Component1>(entities[i]);
		}

		if (i % 3 == 0)
		{
			entityManager.addComponent<Component1>(entities[i]);
			entityManager.addComponent<Component2>(entities[i]);
		}
	}

	int counter = 0;

	entityManager.each<Component0, Component1, Component2>([&counter](const VEngine::Entity *entity, Component0&, Component1&, Component2&)
	{
		++counter;
	});

	for (size_t i = 0; i < 32; ++i)
	{
		if (i % 2 == 0)
		{
			entityManager.destroyEntity(entities[i]);
		}
	}

	counter = 0;

	entityManager.each<Component0, Component1, Component2>([&counter](const VEngine::Entity *entity, Component0&, Component1&, Component2&)
	{
		++counter;
	});

	DummyLogic logic;
	VEngine::Engine engine("Vulkan", logic);
	engine.start();

	return EXIT_SUCCESS;
}