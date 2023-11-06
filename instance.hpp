#pragma once

#include <string>

#include <instance_class.hpp>

class Instance {
	public:
		explicit Instance(InstanceClass classID)
				: m_classID(classID) {}

		void set_parent(Instance* newParent) {
			if (newParent == m_parent || newParent == this) {
				return;
			}

			auto* oldParent = m_parent;
			m_parent = newParent;

			if (oldParent) {
				if (oldParent->m_firstChild == this) {
					oldParent->m_firstChild = m_nextChild;
				}
			}

			if (newParent) {
				if (newParent->m_lastChild) {
					newParent->m_lastChild->m_nextChild = this;
				}
				else {
					newParent->m_firstChild = this;
				}

				newParent->m_lastChild = this;
			}

			m_nextChild = nullptr;
		}

		template <typename Functor>
		void for_each_child(Functor&& func) {
			auto pNextInstance = m_firstChild;

			while (pNextInstance) {
				func(*pNextInstance);
				pNextInstance = pNextInstance->m_nextChild;
			}
		}

		template <typename Functor>
		void for_each_child(Functor&& func) const {
			const_cast<Instance*>(this)->for_each_child(std::move(func));
		}

		const std::string& get_name() const {
			return m_name;
		}

		void set_name(std::string name) {
			m_name = std::move(name);
		}

		InstanceClass get_class_id() const {
			return m_classID;
		}
	private:
		Instance* m_parent = nullptr;
		Instance* m_firstChild = nullptr;
		Instance* m_nextChild = nullptr;
		Instance* m_lastChild = nullptr;

		std::string m_name = "Instance";
		InstanceClass m_classID;
};

