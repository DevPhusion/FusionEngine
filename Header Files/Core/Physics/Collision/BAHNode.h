#pragma once
#include "../../../Objects/Object.h"
#include "DebugCircle.h"

struct PotentialContact {
	Object* obj[2];
};

template<class BoundingAreaClass>
class BAHNode
{
public:
	BAHNode* parent = nullptr;
	BAHNode* children[2] = { nullptr, nullptr };
	BoundingAreaClass area;
	Object* obj = nullptr; //Only leaf can have object, if root -> nullptr
	BAHNode() = default;
	BAHNode(BAHNode* p, const BoundingAreaClass& a, Object* o) : parent(p), area(a), obj(o) {}

	~BAHNode() {
		if (children[0]) delete children[0];
		if (children[1]) delete children[1];
	}

	bool isLeaf() const {
		return (obj != nullptr);
	}

	BAHNode<BoundingAreaClass>* insert(Object* newObject, const BoundingAreaClass& newArea);
	void removeLeaf();

	void recalculateBoundingArea();
	BAHNode<BoundingAreaClass>* searchFor(Object* target);
	bool overlaps(const BAHNode<BoundingAreaClass>* other) const;
	unsigned getPotentialContacts(std::vector<PotentialContact>& contacts) const;
	unsigned getPotentialContactsWith(const BAHNode<BoundingAreaClass>* other, std::vector<PotentialContact>& contacts) const;
	void DrawBoundingArea() const;
};

template<class BoundingAreaClass> 
void BAHNode<BoundingAreaClass>::removeLeaf() {
	if (!parent) {
		obj = nullptr;
		area = BoundingAreaClass();
		return;
	}

	BAHNode<BoundingAreaClass>* sibling = (parent->children[0] == this) ? parent->children[1] : parent->children[0];

	parent->area = sibling->area;
	parent->obj = sibling->obj;
	parent->children[0] = sibling->children[0];
	parent->children[1] = sibling->children[1];

	if (parent->children[0]) parent->children[0]->parent = parent;
	if (parent->children[1]) parent->children[1]->parent = parent;

	sibling->children[0] = nullptr;
	sibling->children[1] = nullptr;
	delete sibling;

	parent->recalculateBoundingArea();
	
	this->children[0] = nullptr;
	this->children[1] = nullptr;
	delete this;
}

template<class BoundingAreaClass> 
BAHNode<BoundingAreaClass>* BAHNode<BoundingAreaClass>::insert(Object* newObject, const BoundingAreaClass& newArea) {
	if (isLeaf()) {
		children[0] = new BAHNode<BoundingAreaClass>(this, area, obj);
		children[1] = new BAHNode<BoundingAreaClass>(this, newArea, newObject);

		this->obj = nullptr;
		recalculateBoundingArea();
		return children[1];
	}
	else {
		if (children[0]->area.getGrowth(newArea) < children[1]->area.getGrowth(newArea)) {
			return children[0]->insert(newObject, newArea);
		}
		else {
			return children[1]->insert(newObject, newArea);
		}
	}
}

template<class BoundingAreaClass>
void BAHNode<BoundingAreaClass>::recalculateBoundingArea() {
	if (isLeaf()) return;

	area = BoundingAreaClass(children[0]->area, children[1]->area);
	if (parent) {
		parent->recalculateBoundingArea();
	}
}

template<class BoundingAreaClass>
bool BAHNode<BoundingAreaClass>::overlaps(const BAHNode<BoundingAreaClass>* other) const {
	return area.overlaps(&other->area);
}

template<class BoundingAreaClass>
unsigned BAHNode<BoundingAreaClass>::getPotentialContacts(std::vector<PotentialContact>& contacts) const {
	if ((children[0] == nullptr && children[1] == nullptr) || isLeaf()) return 0;

	unsigned count = children[0]->getPotentialContactsWith(children[1], contacts);
	count += children[0]->getPotentialContacts(contacts);
	count += children[1]->getPotentialContacts(contacts);

	return count;
}

template<class BoundingAreaClass>
unsigned BAHNode<BoundingAreaClass>::getPotentialContactsWith(const BAHNode<BoundingAreaClass>* other, std::vector<PotentialContact>& contacts) const {
	if (other == nullptr) return 0;
	if (!overlaps(other)) return 0;

	if (isLeaf() && other->isLeaf()) {
		PotentialContact pc;
		pc.obj[0] = obj;
		pc.obj[1] = other->obj;
		contacts.push_back(pc);
		return 1;
	}

	if (other->isLeaf() || (!isLeaf() && area.getSize() >= other->area.getSize())) {
		unsigned count = children[0]->getPotentialContactsWith(other, contacts);
		count += children[1]->getPotentialContactsWith(other, contacts);
		return count;
	}
	else {
		unsigned count = getPotentialContactsWith(other->children[0], contacts);
		count += getPotentialContactsWith(other->children[1], contacts);
		return count;
	}
}

template<class BoundingAreaClass>
BAHNode<BoundingAreaClass>* BAHNode<BoundingAreaClass>::searchFor(Object* target) {
	if (isLeaf()) {
		return (obj == target) ? this : nullptr;
	}

	if (children[0]) {
		BAHNode<BoundingAreaClass>* found = children[0]->searchFor(target);
		if (found != nullptr) {
			return found;
		}
	}
	if (children[1]) {
		BAHNode<BoundingAreaClass>* found = children[1]->searchFor(target);
		if (found != nullptr) {
			return found;
		}
	}

	return nullptr;
}

template<class BoundingAreaClass>
void BAHNode<BoundingAreaClass>::DrawBoundingArea() const {
	DebugCircle::getInstance().DrawCircle(area.center, area.radius, Shader("vertex.txt", "fragment.txt"));

	if (children[0] != nullptr) {
		children[0]->DrawBoundingArea();
	}
	if (children[1] != nullptr) {
		children[1]->DrawBoundingArea();
	}
}