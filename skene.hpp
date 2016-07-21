///	Peter LaValle / gmail
///	skene.hpp
///
/// I place this in "public domain" ... if your lawyers say no to that; get better lawyers
///	... though if you can handle it; feedback and attribution would make my tail wag (but I by no means require such silliness)
///
///	this is a ruthlessly-pure scene graph data structure
/// it tries to do as little as possible, since doing any more makes it hard to integrate
///
/// it needs GLM which tends to work better than any of us was expecting

#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <functional>
#include <list>
#include <string>

#include <assert.h>
#include <stdint.h>

//
static_assert(sizeof(glm::mat4) == (sizeof(float) * 16), "Check sizes");

template<typename D>
struct skene
{
	class node;
	class node
	{
		node(const node&) = delete;
		node& operator=(const node&) = delete;

		friend struct skene;

		template<typename ...ARGS>
		node(struct skene<D>& scene, class skene<D>::node* parent, ARGS&&...args) :
			_scene(scene),
			_parent_node(parent),
			_data(args...)
		{
			_transform._translate = glm::vec3(0);
			_transform._rotate = glm::vec3(0);
			_transform._scale = glm::vec3(1);
		}

		std::list<node> _children;
		node* _parent_node;
	public:

		D _data;
		skene& _scene;

		struct {
			glm::vec3 _translate;
			glm::vec3 _rotate;
			glm::vec3 _scale;
		} _transform;

		/// don't call this yourself
		template<typename ...ARGS>
		node(class skene<D>::node& parent, ARGS&&...args) :
			node(parent._scene, &parent, args...)
		{
		}

		~node(void)
		{
			_children.clear();

			if (nullptr == _parent_node)
				return;

			// remove us from the parent
			const auto end = _parent_node->_children.end();
			const auto found = std::find(_parent_node->_children.begin(), end, *this);
			if (end != found)
				_parent_node->_children.erase(found);
		}

		void append(node& child);

		template<typename ...ARGS>
		node& append(ARGS&&...args)
		{
			_children.emplace_back(*this, args...);

			return _children.back();
		}

		typename std::list<node>::iterator begin(void) { return _children.begin(); }
		typename std::list<node>::iterator end(void) { return _children.end(); }

		/// "folds" a value across all nodes and returns the result
		template<typename I, typename R>
		R foldPull(I& user, const R& leaf, R(*merge)(I&, const node&, D& data, const R& leaf))
		{
			if (_children.empty())
				return merge(user, *this, _data, leaf);

			R result = _children.front().foldPull(user, leaf, merge);

			const auto end = _children.end();
			for (auto it = _children.begin(); (++it) != end;)
			{
				result = (*it).foldPull(user, result, merge);

				assert(end == _children.end() && "Don't adjust the scene while you're iterating over it");
			}

			return merge(user, *this, _data, result);
		}

		bool is_leaf(void) const { return _children.empty(); }
		bool is_root(void) const { return (nullptr == _parent_node); }
		bool not_root(void) const { return (nullptr != _parent_node); }

		glm::mat4 local_to_world(void) const
		{
			// TODO ; slightly inefficent? maybe? fix when all else is working

			glm::mat4 translate = glm::translate(glm::mat4(), _transform._translate);

			glm::mat4 rotate = glm::rotate(glm::mat4(), glm::radians(_transform._rotate[2]), glm::vec3(0, 0, 1));
			rotate = glm::rotate(rotate, glm::radians(_transform._rotate[1]), glm::vec3(0, 1, 0));
			rotate = glm::rotate(rotate, glm::radians(_transform._rotate[0]), glm::vec3(1, 0, 0));

			glm::mat4 scale = glm::scale(glm::mat4(), _transform._scale);

			auto local = translate * rotate * scale;

			PAL_STUB_ASSUME((not_root()) || glm::mat4() == local, "The skene::_root should not be transformed");

			return not_root() ? _parent_node->local_to_world() * local : local;
		}

		//node* parent(void) { return _parent; }

		void push_it(const glm::vec3& direction);

		void remove(void);

		//size_t size(void) const { return foldPull<size_t>(0, [](const node&, const size_t& left) { return 1 + left; }); }

		glm::mat4 world_to_local(void) const { return glm::inverse(local_to_world()); }

		glm::vec3 world_to_local(const glm::vec3& local, const float w = 1) const { return glm::vec3(local_to_world() * glm::vec4(local, w)); }

		bool operator==(const node& other) const{ return this == &other; }

		D* operator->(void) { return &_data; }
	};

	node _root;

	/// constructs the scene and sets the root node's whatnots
	template<typename ...ARGS>
	skene(ARGS&&...args) : _root(*this, nullptr, args...) { }

	~skene(void) { }

	void visit(std::function<bool(node&)> callback)
	{
		struct nested
		{
			static void recursion(std::function<bool(skene<D>::node&)>& callback, node& root)
			{
				if (!callback(root))
				{
					return;
				}

				for (auto& next : root)
				{
					nested::recursion(callback, next);
				}
			}
		};

		for (auto& root : _root)
		{
			nested::recursion(callback, root);
		}
	}
};


template<typename D>
inline
void skene<D>::node::append(typename skene<D>::node& child)
{
	// probably safe, but I'd rather block it
	assert(this != child._parent_node);

	// both must be in the same sceme
	assert(&(_scene) == &(child._scene));

	// we must not descend from child (but they can descend from us)
	for (skene<D>::node* parentage = _parent_node; nullptr != parentage; parentage = parentage->_parent_node)
		assert(parentage != &(child));

	// by implication - we've tested and found that child is not-root so (AFAIK) it must have a valid parent

	// neat; move child between lists without reallocating
	_children.splice(
		_children.end(),
		child._parent_node->_children,
		std::find(child._parent_node->_children.begin(), child._parent_node->_children.end(), child));

	// change the parent pointer
	child._parent_node = this;
}

template<typename D>
inline
void skene<D>::node::remove(void)
{
	assert(not_root());

	// remove us from the parent
	const auto end = _parent_node->_children.end();
	const auto found = std::find(_parent_node->_children.begin(), end, *this);
	if (end != found)
		_parent_node->_children.erase(found);
}

template<typename D>
inline
void skene<D>::node::push_it(const glm::vec3& direction)
{
	_transform._translate += world_to_local(direction, 0);
}
