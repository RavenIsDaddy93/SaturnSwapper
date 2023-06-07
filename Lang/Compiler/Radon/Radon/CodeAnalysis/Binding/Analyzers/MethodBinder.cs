using System.Collections.Generic;
using System.Collections.Immutable;
using Radon.CodeAnalysis.Binding.Semantics;
using Radon.CodeAnalysis.Binding.Semantics.Members;
using Radon.CodeAnalysis.Binding.Semantics.Statements;
using Radon.CodeAnalysis.Symbols;
using Radon.CodeAnalysis.Syntax.Nodes;
using Radon.CodeAnalysis.Syntax.Nodes.Members;

namespace Radon.CodeAnalysis.Binding.Analyzers;

internal sealed class MethodBinder : Binder
{
    internal MethodBinder(Scope? scope) 
        : base(scope)
    {
    }

    internal MethodBinder(Binder binder) 
        : base(binder)
    {
    }

    public override BoundNode Bind(SyntaxNode node, params object[] args)
    {
        var methodContext = new SemanticContext(this, node, Diagnostics);
        if (args.Length != 1 || 
            args[0] is not AbstractMethodSymbol abstractMethodSymbol)
        {
            return new BoundErrorMember(node, methodContext);
        }

        if (abstractMethodSymbol is MethodSymbol methodSymbol)
        {
            return BindMethod(methodContext, (MethodDeclarationSyntax)node, methodSymbol);
        }
        if (abstractMethodSymbol is ConstructorSymbol constructorSymbol)
        {
            return BindConstructor(methodContext, (ConstructorDeclarationSyntax)node, constructorSymbol);
        }
        
        return new BoundErrorMember(node, methodContext);
    }

    private BoundMethod BindMethod(SemanticContext context, MethodDeclarationSyntax syntax, MethodSymbol symbol)
    {
        foreach (var parameter in symbol.Parameters)
        {
            Register(context, parameter);
        }
        
        foreach (var typeParameter in symbol.TypeParameters)
        {
            Register(context, typeParameter);
        }
        
        var body = syntax.Body;
        var statements = body.Statements;
        var statementBinder = new StatementBinder(this);
        var boundStatements = new List<BoundStatement>();
        foreach (var statement in statements)
        {
            var boundStatement = (BoundStatement)statementBinder.Bind(statement, symbol);
            boundStatements.Add(boundStatement);
        }
        
        Diagnostics.AddRange(statementBinder.Diagnostics);
        return new BoundMethod(syntax, symbol, boundStatements.ToImmutableArray(), statementBinder.Locals);
    }
    
    private BoundConstructor BindConstructor(SemanticContext context, ConstructorDeclarationSyntax syntax, ConstructorSymbol symbol)
    {
        foreach (var parameter in symbol.Parameters)
        {
            Register(context, parameter);
        }
        
        var body = syntax.Body;
        var statements = body.Statements;
        var statementBinder = new StatementBinder(this);
        var boundStatements = new List<BoundStatement>();
        foreach (var statement in statements)
        {
            var boundStatement = (BoundStatement)statementBinder.Bind(statement, symbol);
            boundStatements.Add(boundStatement);
        }
        
        Diagnostics.AddRange(statementBinder.Diagnostics);
        return new BoundConstructor(syntax, symbol, boundStatements.ToImmutableArray());
    }
}