#include "instance.hpp"

#include <cstdio>

std::shared_ptr<Instance> Instance::create(InstanceClass classID) {
	return std::make_shared<Instance>(classID);
}

Instance::Instance(InstanceClass classID)
		: m_classID(classID) {}

Instance::~Instance() {
	printf("Deallocating Instance '%s'\n", m_name.c_str());
}

void Instance::set_parent(Instance* newParent) {
	auto oldParent = m_parent.lock();

	if (newParent == oldParent.get() || newParent == this) {
		return;
	}

	m_parent = newParent ? newParent->weak_from_this() : std::weak_ptr<Instance>{};

	if (oldParent) {
		if (oldParent->m_firstChild.get() == this) {
			oldParent->m_firstChild = m_nextChild;
		}
	}

	if (newParent) {
		if (auto lastChild = newParent->m_lastChild.lock()) {
			lastChild->m_nextChild = shared_from_this();
		}
		else {
			newParent->m_firstChild = shared_from_this();
		}

		newParent->m_lastChild = weak_from_this();
	}

	m_nextChild = nullptr;
}

const std::string& Instance::get_name() const {
	return m_name;
}

void Instance::set_name(std::string name) {
	m_name = std::move(name);
}

InstanceClass Instance::get_class_id() const {
	return m_classID;
}

