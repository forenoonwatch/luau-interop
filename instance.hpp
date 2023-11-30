#pragma once

#include <memory>
#include <string>

#include <instance_class.hpp>

class Instance : public std::enable_shared_from_this<Instance> {
	public:
		static std::shared_ptr<Instance> create(InstanceClass);

		explicit Instance(InstanceClass classID);
		~Instance();

		void set_parent(Instance* newParent);

		template <typename Functor>
		void for_each_child(Functor&& func) {
			auto* pNextInstance = m_firstChild.get();

			while (pNextInstance) {
				func(*pNextInstance);
				pNextInstance = pNextInstance->m_nextChild.get();
			}
		}

		template <typename Functor>
		void for_each_child(Functor&& func) const {
			const_cast<Instance*>(this)->for_each_child(std::move(func));
		}

		const std::string& get_name() const;
		void set_name(std::string name);

		InstanceClass get_class_id() const;
	private:
		std::weak_ptr<Instance> m_parent{};
		std::shared_ptr<Instance> m_firstChild{};
		std::shared_ptr<Instance> m_nextChild{};
		std::weak_ptr<Instance> m_prevChild{};
		std::weak_ptr<Instance> m_lastChild{};

		std::string m_name = "Instance";
		InstanceClass m_classID;
};

