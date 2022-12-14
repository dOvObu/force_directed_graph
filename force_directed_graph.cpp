#include <SFML/Graphics.hpp>
#include <fstream>
#include <iostream>
#include <vector>

struct Time
{
	static float GetDelta()
	{
		return _clock.getElapsedTime().asSeconds();
	}

	static void Update()
	{
		_clock.restart();
	}

private:
	static sf::Clock _clock;
};
sf::Clock Time::_clock;

inline float distance(sf::Vector2f const& a, sf::Vector2f const& b)
{
	sf::Vector2f const delta = a - b;
	return std::sqrtf(delta.x * delta.x + delta.y * delta.y);
}

struct Node : sf::Drawable
{
	const float RADIUS = 4.f;

	Node()
		:Node(sf::Vector2f{ 0.f, 0.f })
	{
	}
	Node(float const x, float const y)
		:Node(sf::Vector2f{ x, y })
	{
	}
	Node(sf::Vector2f position)
	{
		_view.setFillColor(sf::Color::Black);
		_view.setRadius(RADIUS);
		_view.setOrigin(RADIUS, RADIUS);
		_view.setPosition(position);
	}
	Node(Node const& node)
	{
		_view = node._view;
	}
	void SetPosition(sf::Vector2f const& position)
	{
		_view.setPosition(position);
	}
	sf::Vector2f GetPosition() const
	{
		return _view.getPosition();
	}
	void ApplyVelocity(sf::Vector2f const velocity)
	{
		_view.move(velocity);
	}
	void ApplyForces(std::vector<sf::Vector2f>& forces)
	{
		if (forces.empty()) return;
		
		sf::Vector2f velocity{ 0.f, 0.f };
		for (sf::Vector2f const& force : forces) velocity += force;
		float const dist = distance({ 0.f, 0.f }, velocity);
		ApplyVelocity(dist > MAX_VELOCITY_MAGNITUDE ? MAX_VELOCITY_MAGNITUDE * (velocity / dist) : velocity);
	}
	void draw(sf::RenderTarget& target, sf::RenderStates states) const override
	{
		target.draw(_view, states);
	}

private:
	sf::CircleShape _view;
	const float MAX_VELOCITY_MAGNITUDE = 300.f;
};

inline float distance(Node const* const a, Node const* const b)
{
	return distance(a->GetPosition(), b->GetPosition());
}

struct Link : sf::Drawable
{
	Link(Node* first, Node* second)
		: _first(first),
		_second(second)
	{
	}
	void draw(sf::RenderTarget& target, sf::RenderStates states) const override
	{
		sf::Vertex const lineVertexes[2] {
			sf::Vertex{_first->GetPosition(), sf::Color::Black },
			sf::Vertex{_second->GetPosition(), sf::Color::Black },
		};
		target.draw(lineVertexes, 2, sf::Lines, states);
	}

	Node* GetFirst() const { return _first; }
	Node* GetSecond() const { return _second; }
private:
	Node* _first;
	Node* _second;
};

struct Graph : sf::Drawable
{
	void AddNodes(std::vector<Node*> const& nodes)
	{
		for (Node* node : nodes) _nodes.push_back(node);
	}
	std::vector<Node*>& GetNodes()
	{
		return _nodes;
	}
	void AddLinks(std::vector<Link*> const& links)
	{
		for (Link* link : links) _links.push_back(link);
	}
	void DeleteLinksAndNodes()
	{
		for (Node const* node : _nodes) delete node;
		for (Link const* link : _links) delete link;
		_nodes.clear();
		_links.clear();
	}
	void Update() const
	{
		std::map<Node*, std::vector<sf::Vector2f>> nodeForces;
		for (Node* node1 : _nodes) nodeForces[node1] = {};

		for (Node* node1 : _nodes)
			for (Node* node2 : _nodes)
				if (node1 != node2)
					nodeForces[node1].push_back(ComputeRepulsiveForce(node1, node2));

		for (Link* link : _links)
		{
			sf::Vector2f force = ComputeAttractionForce(link);
			nodeForces[link->GetFirst()].push_back(-force);
			nodeForces[link->GetSecond()].push_back(force);
		}

		for (auto& nodeAndForces : nodeForces)
			nodeAndForces.first->ApplyForces(nodeAndForces.second);

	}
	void draw(sf::RenderTarget& target, sf::RenderStates states) const override
	{
		for (Node const* node : _nodes) target.draw(*node, states);
		for (Link const* link : _links) target.draw(*link, states);
	}
private:
	float const REPULSION_DISTANCE = 100.f;
	float const ATTRACTION_FORCE = 200.f;
	float const REPULSION_FORCE = 200.f;
	std::vector<Node*> _nodes;
	std::vector<Link*> _links;

	sf::Vector2f ComputeRepulsiveForce(Node const* const node, Node const* const repulsiveNode) const
	{
		float const dist = distance(node, repulsiveNode);
		if (dist > REPULSION_DISTANCE || dist <= FLT_EPSILON) return { 0.f,0.f };

		sf::Vector2f const forceDirection = (node->GetPosition() - repulsiveNode->GetPosition()) / dist;
		float const distanceForce = (REPULSION_DISTANCE - dist) / REPULSION_DISTANCE;
		return REPULSION_FORCE * distanceForce * Time::GetDelta() * forceDirection;
	}

	sf::Vector2f ComputeAttractionForce(Link const* const link) const
	{
		float const dist = distance(link->GetFirst(), link->GetSecond());
		if (dist <= FLT_EPSILON) return { 0.f, 0.f };
		sf::Vector2f const forceDirection = (link->GetFirst()->GetPosition() - link->GetSecond()->GetPosition()) / dist;
		return ATTRACTION_FORCE * 0.5f * Time::GetDelta() * forceDirection;
	}
};

std::istream& operator>>(std::istream& stream, Graph& graph)
{
	size_t nodesCount;
	stream >> nodesCount;

	std::vector<Node*> nodes;
	for (size_t idx = 0; idx < nodesCount; ++idx)
	{
		float x, y;
		stream >> x >> y;
		nodes.push_back(new Node(x, y));
	}

	std::vector<Link*> links;
	while(!stream.eof())
	{
		int first, second;
		stream >> first >> second;
		links.push_back(new Link(nodes[first - 1], nodes[second - 1]));
	}

	graph.AddNodes(nodes);
	graph.AddLinks(links);

	return stream;
}

int main(int argc, char* argv[])
{
	unsigned const WIDTH = 800;
	unsigned const HEIGHT = 600;
	sf::RenderWindow window({ WIDTH, HEIGHT }, "heh");
	Graph graph;
	std::ifstream file("graph_data.txt");
	file >> graph;
	while (window.isOpen())
	{
		sf::Event windowEvent;
		while (window.pollEvent(windowEvent))
		{
			if (windowEvent.type == sf::Event::EventType::Closed)
			{
				window.close();
			}
		}
		graph.Update();
		for (Node* node : graph.GetNodes()) node->SetPosition({
			fmaxf(0.f, fminf(WIDTH, node->GetPosition().x)),
			fmaxf(0.f, fminf(HEIGHT, node->GetPosition().y)),
		});
		Time::Update();
		window.clear(sf::Color::White);
		window.draw(graph);
		window.display();
	}
	graph.DeleteLinksAndNodes();
}
