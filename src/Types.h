#pragma once

#include <memory>
#include <vector>

namespace tomcat {
    namespace model {
        class Node;
        class RandomVariableNode;
        class NodeMetadata;
        class CPD;

        typedef std::shared_ptr<NodeMetadata> MetadataPtr;
        typedef std::shared_ptr<Node> NodePtr;
        typedef std::shared_ptr<RandomVariableNode> RVNodePtr;
        typedef std::vector<NodePtr> NodePtrVec;
        typedef std::vector<RVNodePtr> RVNodePtrVec;
        typedef std::shared_ptr<CPD> CPDPtr;
    }
} // namespace tomcat
