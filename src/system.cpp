//-----------------------------------------------------------------------------
// Once we've written our constraint equations in the symbolic algebra system,
// these routines linearize them, and solve by a modified Newton's method.
// This also contains the routines to detect non-convergence or inconsistency,
// and report diagnostics to the user.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#include <Eigen/SparseQR>

// The solver will converge all unknowns to within this tolerance. This must
// always be much less than LENGTH_EPS, and in practice should be much less.
const double System::CONVERGE_TOLERANCE = (LENGTH_EPS/(1e2));

void System::WriteJacobian(int tag) {
    // Clear all
    mat.param.clear();
    mat.eq.clear();
    mat.B.sym.clear();

    for(Param &p : param) {
        if(p.tag != tag) continue;
        mat.param.push_back(p.h);
    }
    mat.n = mat.param.size();

    for(Equation &e : eq) {
        if(e.tag != tag) continue;
        mat.eq.push_back(&e);
    }
    mat.m = mat.eq.size();
    delete mat.A.sym;
    mat.A.sym = new Eigen::SparseMatrix<Expr *>(mat.m, mat.n);
    mat.A.sym->reserve(Eigen::VectorXi::Constant(mat.n, 10));

    // Fill the param id to index map
    std::map<uint32_t, int> paramToIndex;
    for(int j = 0; j < mat.n; j++) {
        paramToIndex[mat.param[j].v] = j;
    }

    for(size_t i = 0; i < mat.eq.size(); i++) {
        Equation *e = mat.eq[i];
        if(e->tag != tag) continue;
        Expr *f = e->e->FoldConstants();
        f = f->DeepCopyWithParamsAsPointers(&param, &(SK.param));

        List<hParam> paramsUsed = {};
        f->ParamsUsedList(&paramsUsed);

        for(hParam &p : paramsUsed) {
            auto j = paramToIndex.find(p.v);
            if(j == paramToIndex.end()) continue;
            Expr *pd = f->PartialWrt(p);
            pd = pd->FoldConstants();
            if(pd->IsZeroConst()) continue;
            mat.A.sym->insert(i, j->second) = pd;
        }
        paramsUsed.Clear();
        mat.B.sym.push_back(f);
    }
}

void System::EvalJacobian() {
    using namespace Eigen;
    delete mat.A.num;
    mat.A.num = new Eigen::SparseMatrix<double>(mat.m, mat.n);
    int size = mat.A.sym->outerSize();

    for(int k = 0; k < size; k++) {
        for(SparseMatrix <Expr *>::InnerIterator it(*mat.A.sym, k); it; ++it) {
            double value = it.value()->Eval();
            if(EXACT(value == 0.0)) continue;
            mat.A.num->insert(it.row(), it.col()) = value;
        }
    }
    mat.A.num->makeCompressed();
}

bool System::IsDragged(hParam p) {
    hParam *pp;
    for(pp = dragged.First(); pp; pp = dragged.NextAfter(pp)) {
        if(p == *pp) return true;
    }
    return false;
}

Param *System::GetLastParamSubstitution(Param *p) {
    Param *current = p;
    while(current->substd != NULL) {
        current = current->substd;
        if(current == p) {
            // Break the loop
            current->substd = NULL;
            break;
        }
    }
    return current;
}

void System::SortSubstitutionByDragged(Param *p) {
    std::vector<Param *> subsParams;
    Param *by = NULL;
    Param *current = p;
    while(current != NULL) {
        subsParams.push_back(current);
        if(IsDragged(current->h)) {
            by = current;
        }
        current = current->substd;
    }
    if(by == NULL) by = p;
    for(Param *p : subsParams) {
       if(p == by) continue;
       p->substd = by;
       p->tag = VAR_SUBSTITUTED;
    }
    by->substd = NULL;
    by->tag = 0;
}

void System::SubstituteParamsByLast(Expr *e) {
    ssassert(e->op != Expr::Op::PARAM_PTR, "Expected an expression that refer to params via handles");

    if(e->op == Expr::Op::PARAM) {
        Param *p = param.FindByIdNoOops(e->parh);
        if(p != NULL) {
            Param *s = GetLastParamSubstitution(p);
            if(s != NULL) {
                e->parh = s->h;
            }
        }
    } else {
        int c = e->Children();
        if(c >= 1) {
            SubstituteParamsByLast(e->a);
            if(c >= 2) SubstituteParamsByLast(e->b);
        }
    }
}

void System::SolveBySubstitution() {
    for(auto &teq : eq) {
        Expr *tex = teq.e;

        if(tex->op    == Expr::Op::MINUS &&
           tex->a->op == Expr::Op::PARAM &&
           tex->b->op == Expr::Op::PARAM)
        {
            hParam a = tex->a->parh;
            hParam b = tex->b->parh;
            if(!(param.FindByIdNoOops(a) && param.FindByIdNoOops(b))) {
                // Don't substitute unless they're both solver params;
                // otherwise it's an equation that can be solved immediately,
                // or an error to flag later.
                continue;
            }

            if(a.v == b.v) {
                teq.tag = EQ_SUBSTITUTED;
                continue;
            }

            Param *pa = param.FindById(a);
            Param *pb = param.FindById(b);

            // Take the last substitution of parameter a
            // This resulted in creation of substitution chains
            Param *last = GetLastParamSubstitution(pa);
            last->substd = pb;
            last->tag = VAR_SUBSTITUTED;

            if(pb->substd != NULL) {
                // Break the loops
                GetLastParamSubstitution(pb);
                // if b loop was broken
                if(pb->substd == NULL) {
                    // Clear substitution
                    pb->tag = 0;
                }
            }
            teq.tag = EQ_SUBSTITUTED;
        }
    }

    //
    for(Param &p : param) {
        SortSubstitutionByDragged(&p);
    }

    // Substitute all the equations
    for(auto &req : eq) {
        SubstituteParamsByLast(req.e);
    }

    // Substitute all the parameters with last substitutions
    for(auto &p : param) {
        if(p.substd == NULL) continue;
        p.substd = GetLastParamSubstitution(p.substd);
    }
}

//-----------------------------------------------------------------------------
// Calculate the rank of the Jacobian matrix
//-----------------------------------------------------------------------------
int System::CalculateRank() {
    using namespace Eigen;
    if(mat.n == 0 || mat.m == 0) return 0;
    SparseQR <SparseMatrix<double>, COLAMDOrdering<int>> solver;
    solver.compute(*mat.A.num);
    int result = solver.rank();
    return result;
}

bool System::TestRank(int *dof) {
    EvalJacobian();
    int jacobianRank = CalculateRank();
    // We are calculating dof based on real rank, not mat.m.
    // Using this approach we can calculate real dof even when redundant is allowed.
    if(dof != NULL) *dof = mat.n - jacobianRank;
    return jacobianRank == mat.m;
}

bool System::SolveLinearSystem(const Eigen::SparseMatrix <double> &A,
                               const Eigen::VectorXd &B, Eigen::VectorXd *X)
{
    if(A.outerSize() == 0) return true;
    using namespace Eigen;
    SparseQR<SparseMatrix<double>, COLAMDOrdering<int>> solver;
    //SimplicialLDLT<SparseMatrix<double>> solver;
    solver.compute(A);
    *X = solver.solve(B);
    return (solver.info() == Success);
}

bool System::SolveLeastSquares() {
    using namespace Eigen;
    // Scale the columns; this scale weights the parameters for the least
    // squares solve, so that we can encourage the solver to make bigger
    // changes in some parameters, and smaller in others.
    mat.scale = VectorXd(mat.n);
    for(int c = 0; c < mat.n; c++) {
        if(IsDragged(mat.param[c])) {
            // It's least squares, so this parameter doesn't need to be all
            // that big to get a large effect.
            mat.scale[c] = 1/20.0;
        } else {
            mat.scale[c] = 1;
        }
        }

    int size = mat.A.sym->outerSize();
    for(int k = 0; k < size; k++) {
        for(SparseMatrix<double>::InnerIterator it(*mat.A.num, k); it; ++it) {
            it.valueRef() *= mat.scale[it.col()];
            }
        }

    SparseMatrix <double> AAt = *mat.A.num * mat.A.num->transpose();
    AAt.makeCompressed();
    VectorXd z(mat.n);

    if(!SolveLinearSystem(AAt, mat.B.num, &z)) return false;

    mat.X = mat.A.num->transpose() * z;

    for(int c = 0; c < mat.n; c++) {
        mat.X[c] *= mat.scale[c];
    }
    return true;
}

bool System::NewtonSolve(int tag) {

    int iter = 0;
    bool converged = false;
    int i;

    // Evaluate the functions at our operating point.
    mat.B.num = Eigen::VectorXd(mat.m);
    for(i = 0; i < mat.m; i++) {
        mat.B.num[i] = (mat.B.sym[i])->Eval();
    }
    do {
        // And evaluate the Jacobian at our initial operating point.
        EvalJacobian();

        if(!SolveLeastSquares()) break;

        // Take the Newton step;
        //      J(x_n) (x_{n+1} - x_n) = 0 - F(x_n)
        for(i = 0; i < mat.n; i++) {
            Param *p = param.FindById(mat.param[i]);
            p->val -= mat.X[i];
            if(IsReasonable(p->val)) {
                // Very bad, and clearly not convergent
                return false;
            }
        }

        // Re-evalute the functions, since the params have just changed.
        for(i = 0; i < mat.m; i++) {
            mat.B.num[i] = (mat.B.sym[i])->Eval();
        }
        // Check for convergence
        converged = true;
        for(i = 0; i < mat.m; i++) {
            if(IsReasonable(mat.B.num[i])) {
                return false;
            }
            if(fabs(mat.B.num[i]) > CONVERGE_TOLERANCE) {
                converged = false;
                break;
            }
        }
    } while(iter++ < 50 && !converged);

    return converged;
}

void System::WriteEquationsExceptFor(hConstraint hc, Group *g) {
    // Generate all the equations from constraints in this group
    for(auto &con : SK.constraint) {
        ConstraintBase *c = &con;
        if(c->group != g->h) continue;
        if(c->h == hc) continue;

        if(c->HasLabel() && c->type != Constraint::Type::COMMENT &&
                g->allDimsReference)
        {
            // When all dimensions are reference, we adjust them to display
            // the correct value, and then don't generate any equations.
            c->ModifyToSatisfy();
            continue;
        }
        if(g->relaxConstraints && c->type != Constraint::Type::POINTS_COINCIDENT) {
            // When the constraints are relaxed, we keep only the point-
            // coincident constraints, and the constraints generated by
            // the entities and groups.
            continue;
        }

        c->GenerateEquations(&eq);
    }
    // And the equations from entities
    for(auto &ent : SK.entity) {
        EntityBase *e = &ent;
        if(e->group != g->h) continue;

        e->GenerateEquations(&eq);
    }
    // And from the groups themselves
    g->GenerateEquations(&eq);
}

void System::FindWhichToRemoveToFixJacobian(Group *g, List<hConstraint> *bad, bool forceDofCheck) {
    auto time = GetMilliseconds();
    g->solved.timeout = false;
    int a;

    for(a = 0; a < 2; a++) {
        for(auto &con : SK.constraint) {
            if((GetMilliseconds() - time) > g->solved.findToFixTimeout) {
                g->solved.timeout = true;
                return;
            }

            ConstraintBase *c = &con;
            if(c->group != g->h) continue;
            if((c->type == Constraint::Type::POINTS_COINCIDENT && a == 0) ||
               (c->type != Constraint::Type::POINTS_COINCIDENT && a == 1))
            {
                // Do the constraints in two passes: first everything but
                // the point-coincident constraints, then only those
                // constraints (so they appear last in the list).
                continue;
            }

            param.ClearTags();
            eq.Clear();
            WriteEquationsExceptFor(c->h, g);
            eq.ClearTags();

            // It's a major speedup to solve the easy ones by substitution here,
            // and that doesn't break anything.
            if(!forceDofCheck) {
                SolveBySubstitution();
            }

            WriteJacobian(0);
            EvalJacobian();

            int rank = CalculateRank();
            if(rank == mat.m) {
                // We fixed it by removing this constraint
                bad->Add(&(c->h));
            }
        }
    }
}

SolveResult System::Solve(Group *g, int *rank, int *dof, List<hConstraint> *bad,
                          bool andFindBad, bool andFindFree, bool forceDofCheck)
{
    WriteEquationsExceptFor(Constraint::NO_CONSTRAINT, g);

    bool rankOk;

/*
    int x;
    dbp("%d equations", eq.n);
    for(x = 0; x < eq.n; x++) {
        dbp("  %.3f = %s = 0", eq[x].e->Eval(), eq[x].e->Print());
    }
    dbp("%d parameters", param.n);
    for(x = 0; x < param.n; x++) {
        dbp("   param %08x at %.3f", param[x].h.v, param[x].val);
    } */

    // All params and equations are assigned to group zero.
    param.ClearTags();
    eq.ClearTags();

    // Since we are suppressing dof calculation or allowing redundant, we
    // can't / don't want to catch result of dof checking without substitution
    if(g->suppressDofCalculation || g->allowRedundant || !forceDofCheck) {
        SolveBySubstitution();
    }

    // Before solving the big system, see if we can find any equations that
    // are soluble alone. This can be a huge speedup. We don't know whether
    // the system is consistent yet, but if it isn't then we'll catch that
    // later.
    int alone = 1;
    for(auto &e : eq) {
        if(e.tag != 0)
            continue;

        hParam hp = e.e->ReferencedParams(&param);
        if(hp == Expr::NO_PARAMS) continue;
        if(hp == Expr::MULTIPLE_PARAMS) continue;

        Param *p = param.FindById(hp);
        if(p->tag != 0) continue; // let rank test catch inconsistency

        e.tag  = alone;
        p->tag = alone;
        WriteJacobian(alone);
        if(!NewtonSolve(alone)) {
            // We don't do the rank test, so let's arbitrarily return
            // the DIDNT_CONVERGE result here.
            rankOk = true;
            // Failed to converge, bail out early
            goto didnt_converge;
        }
        alone++;
    }

    // Now write the Jacobian for what's left, and do a rank test; that
    // tells us if the system is inconsistently constrained.
    WriteJacobian(0);
    // Clear dof value in order to have indication when dof is actually not calculated
    if(dof != NULL) *dof = -1;
    // We are suppressing or allowing redundant, so we no need to catch unsolveable + redundant
    rankOk = (!g->suppressDofCalculation && !g->allowRedundant) ? TestRank(dof) : true;

    // And do the leftovers as one big system
    if(!NewtonSolve(0)) {
        goto didnt_converge;
    }

    // Here we are want to calculate dof even when redundant is allowed, so just handle suppressing
    rankOk = (!g->suppressDofCalculation) ? TestRank(dof) : true;
    if(!rankOk) {
        if(andFindBad) FindWhichToRemoveToFixJacobian(g, bad, forceDofCheck);
    } else {
        MarkParamsFree(andFindFree);
    }
    // System solved correctly, so write the new values back in to the
    // main parameter table.
    for(auto &p : param) {
        double val;
        if(p.tag == VAR_SUBSTITUTED) {
            val = p.substd->val;
        } else {
            val = p.val;
        }
        Param *pp = SK.GetParam(p.h);
        pp->val = val;
        pp->known = true;
        pp->free  = p.free;
    }
    return rankOk ? SolveResult::OKAY : SolveResult::REDUNDANT_OKAY;

didnt_converge:
    SK.constraint.ClearTags();
    for(size_t i = 0; i < mat.eq.size(); i++) {
        if(fabs(mat.B.num[i]) > CONVERGE_TOLERANCE || IsReasonable(mat.B.num[i])) {
            // This constraint is unsatisfied.
            if(!mat.eq[i]->h.isFromConstraint()) continue;

            hConstraint hc = mat.eq[i]->h.constraint();
            ConstraintBase *c = SK.constraint.FindByIdNoOops(hc);
            if(!c) continue;
            // Don't double-show constraints that generated multiple
            // unsatisfied equations
            if(!c->tag) {
                bad->Add(&(c->h));
                c->tag = 1;
            }
        }
    }

    return rankOk ? SolveResult::DIDNT_CONVERGE : SolveResult::REDUNDANT_DIDNT_CONVERGE;
}

SolveResult System::SolveRank(Group *g, int *rank, int *dof, List<hConstraint> *bad,
                              bool andFindBad, bool andFindFree)
{
    WriteEquationsExceptFor(Constraint::NO_CONSTRAINT, g);

    // All params and equations are assigned to group zero.
    param.ClearTags();
    eq.ClearTags();

    // Now write the Jacobian, and do a rank test; that
    // tells us if the system is inconsistently constrained.
    WriteJacobian(0);

    bool rankOk = TestRank(dof);
    if(!rankOk) {
        // When we are testing with redundant allowed, we don't want to have additional info
        // about redundants since this test is working only for single redundant constraint
        if(!g->suppressDofCalculation && !g->allowRedundant) {
            if(andFindBad) FindWhichToRemoveToFixJacobian(g, bad, true);
        }
    } else {
        MarkParamsFree(andFindFree);
    }
    return rankOk ? SolveResult::OKAY : SolveResult::REDUNDANT_OKAY;
}

void System::Clear() {
    entity.Clear();
    param.Clear();
    eq.Clear();
    dragged.Clear();
    delete mat.A.num;
    mat.A.num = NULL;
    delete mat.A.sym;
    mat.A.sym = NULL;
}

void System::MarkParamsFree(bool find) {
    // If requested, find all the free (unbound) variables. This might be
    // more than the number of degrees of freedom. Don't always do this,
    // because the display would get annoying and it's slow.
    for(auto &p : param) {
        p.free = false;

        if(find) {
            if(p.tag == 0) {
                p.tag = VAR_DOF_TEST;
                WriteJacobian(0);
                EvalJacobian();
                int rank = CalculateRank();
                if(rank == mat.m) {
                    p.free = true;
                }
                p.tag = 0;
            }
        }
    }
}

