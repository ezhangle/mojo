Sky DOM APIs
============

```dart
// ELEMENT TREE API

abstract class Node extends EventTarget {
  @override
  external List<EventTarget> getEventDispatchChain(); // O(N) in number of ancestors across shadow trees
  // implements EventTarget.getEventDispatchChain()
  // returns the event dispatch chain (including handling shadow trees)

  external Root get owner; // O(1)

  external ParentNode get parentNode; // O(1)
  Element get parentElement {
    if (parentNode is Element)
      return parentNode as Element;
    return null;
  }

  external Node get previousSibling; // O(1)
  Element get previousElementSibling {
    var result = previousSibling;
    while (result != null && result is! Element)
      result = result.previousSibling;
    return result as Element;
  }

  external Node get nextSibling; // O(1)
  Element get nextElementSibling {
    var result = nextSibling;
    while (result != null && result is! Element)
      result = result.nextSibling;
    return result as Element;
  }

  // TODO(ianh): rename insertBefore() and insertAfter() since the Web
  // has an insertBefore() that means something else. What's a good
  // name, though?

  external void _insertBefore(Node node); // O(N) in number of descendants
  // node must be Text or Element, parentNode must be non-null
  void insertBefore(List nodes) {
    List.forEach((node) {
      if (node is String)
        node = new Text(node);
      _insertBefore(node);
    });    
  }

  external void _insertAfter(Node node); // O(N) in number of arguments plus all their descendants
  // node must be Text or Element, parentNode must be non-null
  void insertAfter(List nodes) {
    var lastNode = this;
    List.forEach((node) {
      if (node is String)
        node = new Text(node);
      lastNode._insertAfter(node);
      lastNode = node;
    });
  }

  void replaceWith(List nodes) {
    if (nextSibling != null) {
      var anchor = nextSibling;
      remove(); // parentNode can't be null here, so this won't throw
      anchor.insertBefore(nodes);
    } else {
      var anchor = parentNode;
      remove(); // throws if parentNode is null
      anchor.append(nodes);
    }
  }

  external void remove(); // O(N) in number of descendants
  // parentNode must be non-null

  // called when parentNode changes
  // this is why insertBefore(), append(), et al, are O(N) -- the whole affected subtree is walked
  // mutating the element tree from within this is strongly discouraged, since it will result in the
  // callbacks being invoked while the element tree is in a different state than implied by the callbacks
  external void parentChangedCallback(ParentNode oldParent, ParentNode newParent); // O(N) in descendants
  // default implementation calls attached/detached
  void attachedCallback() { }
  void detachedCallback() { }

  external List<ContentElement> getDestinationInsertionPoints(); // O(N) in number of insertion points the node is in
  // returns the <content> elements to which this element was distributed

  external Node cloneNode({bool deep: false}); // O(1) if deep=false, O(N) in the number of descendants if deep=true

  external ElementStyleDeclarationList get style; // O(1)
  // for nodes that aren't in the ApplicationRoot's composed tree,
  // returns null (so in particular orphaned subtrees and nodes in
  // module Roots don't have one, nor do shadow tree Roots)
  // also always returns null for ContentElement elements
  //  -- should be (lazily) updated when the node's parent chain
  //     changes (same time as, e.g., the id hashtable is marked
  //     dirty)

  external RenderNode get renderNode; // O(1)
  // this will be null until the first time it is rendered
  // it becomes null again when it is taken out of the rendering (see style.md)

  Type getLayoutManager() => null; // O(1)

  void resetLayoutManager() { // O(1)
    if (renderNode != null) {
      renderNode._layoutManager = null;
      renderNode._needsManager = true;
    }
  }
}

abstract class ParentNode extends Node {
  external Node get firstChild; // O(1)
  Element get firstElementChild {
    var result = firstChild;
    while (result != null && result is! Element)
      result = result.nextSibling;
    return result as Element;
  }

  external Node get lastChild; // O(1)
  Element get lastElementChild {
    var result = lastChild;
    while (result != null && result is! Element)
      result = result.previousSibling;
    return result as Element;
  }

  // Returns a new List every time.
  external List<Node> getChildren(); // O(N) in number of child nodes
  List<Element> getChildElements() {
    // that the following works without a cast is absurd
    return getChildren().where((node) => node is Element).toList();
  }

  external void _appendChild(Node node); // O(N) in number of descendants
  // node must be Text or Element
  void appendChild(node) {
    if (node is String)
      node = new Text(node);
    _appendChild(node);
  }
  void append(List nodes) {
    nodes.forEach(appendChild);
  }

  external void _prependChild(Node node); // O(N) in number of descendants
  // node must be Text or Element
  void prependChild(node) {
    if (node is String)
      node = new Text(node);
    _prependChild(node);
  }
  void prepend(List nodes) {
    // note: not implemented in terms of _prependChild()
    if (firstChild != null)
      firstChild.insertBefore(nodes);
    else
      append(nodes);
  }

  external void removeChildren(); // O(N) in number of descendants
  void setChild(node) {
    removeChildren();
    appendChild(node);
  }
  void setChildren(List nodes) {
    removeChildren();
    append(nodes);
  }
}

class Attr {
  const Attr (this.name, [this.value = '']); // O(1)
  final String name; // O(1)
  final String value; // O(1)
}

// @hasShadow annotation for registering elements
class _HasShadow {
  const _HasShadow();
}
const hasShadow = const _HasShadow();

abstract class Element extends ParentNode {
  Element({Map<String, String> attributes: null,
           List children: null,
           Module hostModule: null}) { // O(M+N), M = number of attributes, N = number of children nodes plus all their descendants
    var shadowClass = reflectClass(hasShadow.runtimeType);
    var shadowAnnotations = reflect(this).type.metadata.where((mirror) => mirror.type == shadowClass);
    if (shadowAnnotations.length > 2)
      throw new StateError('@hasShadow specified multiple times on ' + currentMirrorSystem().getName(reflectClass(this.runtimeType).simpleName));
    bool needsShadow = shadowAnnotations.length == 1;
    if (children != null)
      children = children.map((node) => node is String ? new Text(node) : node).toList();
    this._initElement(attributes, children, hostModule, needsShadow);
  }
  external void _initElement(Map<String, String> attributes, List children, Module hostModule, bool needsShadow);
  // initialises the internal attributes table, which is a ordered list
  // appends the given children nodes
  // children must be Text or Element
  // if needsShadow is true, creates a shadow tree

  external bool hasAttribute(String name); // O(N) in number of attributes
  external String getAttribute(String name); // O(N) in number of attributes
  external void setAttribute(String name, [String value = '']); // O(N) in number of attributes
  external void removeAttribute(String name); // O(N) in number of attributes
  // calling setAttribute() with a null value removes the attribute
  // (calling it without a value sets it to the empty string)

  // Returns a new Array and new Attr instances every time.
  external List<Attr> getAttributes(); // O(N) in number of attributes

  external Root get shadowRoot; // O(1)
  // returns the shadow root

  void endTagParsedCallback() { }
  void attributeChangedCallback(String name, String oldValue, String newValue) { }
  // name will never be null when this is called by sky

  // TODO(ianh): does a node ever need to know when it's been redistributed?

  @override
  Type getLayoutManager() { // O(1)
    if (renderNode)
      return renderNode.getProperty(phDisplay);
    return super.getLayoutManager();
  }
}

class Text extends Node {
  external Text([String value = '']); // O(1)

  external String get value; // O(1)
  external void set (String value); // O(1)

  void valueChangedCallback(String oldValue, String newValue) { }

  @override
  Type getLayoutManager() => TextLayoutManager; // O(1)
}

class Fragment extends ParentNode {
  Fragment({List children}); // O(N) in number of arguments plus all their descendants
  // children must be String, Text, or Element
}

class Root extends ParentNode {
  Root({List children: null, this.host}) { // O(N) in number of children nodes plus all their descendants
    if (children != null)
      children = children.map((node) => node is String ? new Text(node) : node).toList();
    this._initRoot(children);
  }
  external void _initRoot(List children);
  // appends the given children nodes
  // children must be Text or Element

  final Element host;

  external Element findId(String id); // O(1)
  // throws if id is null
}

class ApplicationRoot extends Root {
  ApplicationRoot ({List children}) : super(children: children); // O(N) in number of children nodes arguments plus all their descendants

  @override
  Type getLayoutManager() => rootLayoutManager; // O(1)
}

Type rootLayoutManager = BlockLayoutManager; // O(1)

class SelectorQuery {
  external SelectorQuery(String selector); // O(F()) where F() is the complexity of the selector

  external bool matches(Element element); // O(F())
  external Element find(Node root); // O(N*F())+O(M) where N is the number of descendants and M the average depth of the tree
  external List<Element> findAll(Node root); // O(N*F())+O(N*M) where N is the number of descendants and M the average depth of the tree
  // find() and findAll() throw if the root is not one of the following:
  //  - Element
  //  - Fragment
  //  - Root
}
```
