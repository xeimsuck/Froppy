#pragma once
#include <memory>
#include "visitor.hpp"
#include "../scanner/token.hpp"

namespace cfp::ast {
    class visitor;

    namespace expr {
        struct expression {
            virtual ~expression() = default;

            virtual scan::literal accept(visitor& visitor) = 0;
        };

        struct binary final : expression {
            binary(std::unique_ptr<expression> left, std::unique_ptr<expression> right, scan::token oper);

            scan::literal accept(visitor& visitor) override;

            std::unique_ptr<expression> left;
            std::unique_ptr<expression> right;
            scan::token oper;
        };

        struct grouping final : expression {
            explicit grouping(std::unique_ptr<expression> expr);

            scan::literal accept(visitor &visitor) override;

            std::unique_ptr<expression> expr;
        };

        struct literal final : expression {
            explicit literal(scan::literal value);

            scan::literal accept(visitor &visitor) override;

            scan::literal value;
        };

        struct unary final : expression {
            explicit unary(std::unique_ptr<expression> expr, scan::token oper);

            scan::literal accept(visitor &visitor) override;

            std::unique_ptr<expression> expr;
            scan::token oper;
        };
    }
}