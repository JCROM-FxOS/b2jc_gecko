/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsRootAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsReadableUtils.h"
#include "nsILink.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsIURI.h"
#include "nsIDocShellTreeItem.h"
#include "nsIXULDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMNSHTMLSelectElement.h"
#include "nsIAccessibleSelectable.h"
#include "nsIDOMHTMLCollection.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsXPIDLString.h"

NS_INTERFACE_MAP_BEGIN(nsRootAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFormListener)
NS_INTERFACE_MAP_END_INHERITING(nsAccessible)


NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsAccessible);
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsAccessible);

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIWeakReference* aShell):nsAccessible(nsnull,aShell), nsDocAccessibleMixin(aShell)
{
  mListener = nsnull;
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  NS_ASSERTION(shell,"Shell is gone!!! What are we doing here?");

  shell->GetDocument(getter_AddRefs(mDocument));
  mDOMNode = do_QueryInterface(mDocument);

  nsLayoutAtoms::AddRefAtoms();
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsRootAccessible::~nsRootAccessible()
{
  nsLayoutAtoms::ReleaseAtoms();
  RemoveAccessibleEventListener(mListener);
}

  /* attribute wstring accName; */
NS_IMETHODIMP nsRootAccessible::GetAccName(nsAWritableString& aAccName) 
{ 
  return GetTitle(aAccName);
}

// helpers
nsIFrame* nsRootAccessible::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell)
    return nsnull;

  nsIFrame* root = nsnull;
  if (shell) 
    shell->GetRootFrame(&root);

  return root;
}

void nsRootAccessible::GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  *aRelativeFrame = GetFrame();
  (*aRelativeFrame)->GetRect(aBounds);
}

/* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  *aAccParent = nsnull;
  return NS_OK;
}

/* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsRootAccessible::GetAccRole(PRUint32 *aAccRole) 
{ 
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) {
    *aAccRole = 0;
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresContext> context; 
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsISupports> container;
  context->GetContainer(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem, docTreeItem(do_QueryInterface(container));
    if (docTreeItem) {
      docTreeItem->GetSameTypeParent(getter_AddRefs(parentTreeItem));
      // Basically, if this docshell has a parent of the same type, it's a frame
      if (parentTreeItem) {
        *aAccRole = ROLE_PANE;
        return NS_OK;
      }
    }
  }

  *aAccRole = ROLE_CLIENT;
  return NS_OK;  
}

NS_IMETHODIMP nsRootAccessible::GetAccValue(nsAWritableString& aAccValue)
{
  return GetURL(aAccValue);
}

/* void addAccessibleEventListener (in nsIAccessibleEventListener aListener); */
NS_IMETHODIMP nsRootAccessible::AddAccessibleEventListener(nsIAccessibleEventListener *aListener)
{
  if (!mListener)
  {
    // add an event listener to the document
    nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
    if (!shell)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocument> document;
    shell->GetDocument(getter_AddRefs(document));

    // use AddEventListener from the nsIDOMEventTarget interface
    nsCOMPtr<nsIDOMEventTarget> target;
    if (NS_SUCCEEDED(document->QueryInterface(NS_GET_IID(nsIDOMEventTarget), getter_AddRefs(target))) && target)
    {
      nsresult rv = NS_OK;
      // we're a DOMEventListener now!!
      nsCOMPtr<nsIDOMEventListener> listener;
      rv = this->QueryInterface( NS_GET_IID(nsIDOMEventListener), getter_AddRefs(listener) );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to QI");
      // capture DOM focus events 
      rv = target->AddEventListener( NS_LITERAL_STRING("focus") , listener, PR_TRUE );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      // capture Form change events 
      rv = target->AddEventListener( NS_LITERAL_STRING("change") , listener, PR_TRUE );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      // add ourself as a CheckboxStateChange listener ( custom event fired in nsHTMLInputElement.cpp )
      rv = target->AddEventListener( NS_LITERAL_STRING("CheckboxStateChange") , listener, PR_TRUE );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      // add ourself as a RadiobuttonStateChange listener ( custom event fired in nsHTMLInputElement.cpp )
      rv = target->AddEventListener( NS_LITERAL_STRING("RadiobuttonStateChange") , listener, PR_TRUE );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
    }
  }

  // create a weak reference to the listener
  mListener =  aListener;

  return NS_OK;
}

/* void removeAccessibleEventListener (in nsIAccessibleEventListener aListener); */
NS_IMETHODIMP nsRootAccessible::RemoveAccessibleEventListener(nsIAccessibleEventListener *aListener)
{
  if (mListener)
  {
     nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
     nsCOMPtr<nsIDocument> document;
     if (!shell)
       return NS_OK;

     shell->GetDocument(getter_AddRefs(document));

     nsCOMPtr<nsIDOMEventReceiver> erP;
     if (NS_SUCCEEDED(document->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP))) && erP)
     {
        nsresult rv = erP->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener *, this), NS_GET_IID(nsIDOMFocusListener));
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
     }
  }

  mListener = nsnull;

  return NS_OK;
}

// --------------- nsIDOMEventListener Methods (3) ------------------------

NS_IMETHODIMP nsRootAccessible::HandleEvent(nsIDOMEvent* aEvent)
{
  if (mListener) {
    nsCOMPtr<nsIDOMEventTarget> t;
    aEvent->GetOriginalTarget(getter_AddRefs(t));
  
    nsCOMPtr<nsIContent> content(do_QueryInterface(t));  
    if (!content)
      return NS_OK;

    nsAutoString eventType;
    aEvent->GetType(eventType);

    // the "focus" type is pulled from nsDOMEvent.cpp
    if ( eventType.EqualsIgnoreCase("focus") ) {
      if (mCurrentFocus == content)
        return NS_OK;
      mCurrentFocus = content;
    }

    nsIFrame* frame = nsnull;
    nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
    shell->GetPrimaryFrameFor(content, &frame);
    if (!frame)
      return NS_OK;

    nsCOMPtr<nsIAccessible> a;
    frame->GetAccessible(getter_AddRefs(a));
    if (!a)
      a = do_QueryInterface(content);

    if (!a) {
      // is it a link?
      nsCOMPtr<nsILink> link(do_QueryInterface(content));
      if (link) {
        #ifdef DEBUG
        printf("focus link!\n");
        #endif
        nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
        if (node)
          a = new nsHTMLLinkAccessible(node, mPresShell);
      }
    }

    if (a) {
      if ( eventType.EqualsIgnoreCase("focus") ) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, a);
      }
      else if ( eventType.EqualsIgnoreCase("change") ) {
        nsCOMPtr<nsIDOMNSHTMLSelectElement> select(do_QueryInterface(content));
        if ( select ) {
          mListener->HandleEvent(nsIAccessibleEventListener::EVENT_SELECTION, a);
        }
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, a);
      }
      else if ( eventType.EqualsIgnoreCase("CheckboxStateChange") ) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, a);
      }
      else if ( eventType.EqualsIgnoreCase("RadiobuttonStateChange") ) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, a);
      }
    }
  }
  return NS_OK;
}

// ------- nsIDOMFocusListener Methods (1) -------------

NS_IMETHODIMP nsRootAccessible::Focus(nsIDOMEvent* aEvent) 
{ 
  return HandleEvent(aEvent);
}

NS_IMETHODIMP nsRootAccessible::Blur(nsIDOMEvent* aEvent) { return NS_OK; }

// ------- nsIDOMFormListener Methods (5) -------------

NS_IMETHODIMP nsRootAccessible::Submit(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Reset(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Change(nsIDOMEvent* aEvent)
{
  // get change events when the form elements changes its state, checked->not,
  //  deleted text, new text, change in selection for list/combo boxes
  // this may be the event that we have the individual Accessible objects
  //  handle themselves -- have list/combos figure out the change in selection
  //  have textareas and inputs fire a change of state etc...
  return HandleEvent(aEvent);
}

// gets Select events when text is selected in a textarea or input
NS_IMETHODIMP nsRootAccessible::Select(nsIDOMEvent* aEvent) { return NS_OK; }

// gets Input events when text is entered or deleted in a textarea or input
NS_IMETHODIMP nsRootAccessible::Input(nsIDOMEvent* aEvent) { return NS_OK; }

// ------- nsIAccessibleDocument Methods (5) ---------------

NS_IMETHODIMP nsRootAccessible::GetURL(nsAWritableString& aURL)
{
  return nsDocAccessibleMixin::GetURL(aURL);
}

NS_IMETHODIMP nsRootAccessible::GetTitle(nsAWritableString& aTitle)
{
  return nsDocAccessibleMixin::GetTitle(aTitle);
}

NS_IMETHODIMP nsRootAccessible::GetMimeType(nsAWritableString& aMimeType)
{
  return nsDocAccessibleMixin::GetMimeType(aMimeType);
}

NS_IMETHODIMP nsRootAccessible::GetDocType(nsAWritableString& aDocType)
{
  return nsDocAccessibleMixin::GetDocType(aDocType);
}

NS_IMETHODIMP nsRootAccessible::GetNameSpaceURIForID(PRInt16 aNameSpaceID, nsAWritableString& aNameSpaceURI)
{
  return nsDocAccessibleMixin::GetNameSpaceURIForID(aNameSpaceID, aNameSpaceURI);
}

NS_IMETHODIMP nsRootAccessible::GetDocument(nsIDocument **doc)
{
  return nsDocAccessibleMixin::GetDocument(doc);
}


nsDocAccessibleMixin::nsDocAccessibleMixin(nsIDocument *aDoc):mDocument(aDoc)
{
}

nsDocAccessibleMixin::nsDocAccessibleMixin(nsIWeakReference *aPresShell)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aPresShell));
  NS_ASSERTION(shell,"Shell is gone!!! What are we doing here?");
  shell->GetDocument(getter_AddRefs(mDocument));
}

nsDocAccessibleMixin::~nsDocAccessibleMixin()
{
}

NS_IMETHODIMP nsDocAccessibleMixin::GetURL(nsAWritableString& aURL)
{ 
  nsCOMPtr<nsIURI> pURI;
  mDocument->GetDocumentURL(getter_AddRefs(pURI));
  nsXPIDLCString path;
  pURI->GetSpec(getter_Copies(path));
  aURL.Assign(NS_ConvertUTF8toUCS2(path).get());
  //XXXaaronl Need to use CopyUTF8toUCS2(nsDependentCString(path), aURL); when it's written
  
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetTitle(nsAWritableString& aTitle)
{
  // This doesn't leak - we don't own the const pointer that's returned
  aTitle = *(mDocument->GetDocumentTitle());
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetMimeType(nsAWritableString& aMimeType)
{
  if (mDocument)
    return mDocument->GetContentType(aMimeType); 
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetDocType(nsAWritableString& aDocType)
{
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMDocumentType> docType;

  if (xulDoc) {
    aDocType = NS_LITERAL_STRING("window"); // doctype not implemented for XUL at time of writing - causes assertion
    return NS_OK;
  }
  else if (domDoc && NS_SUCCEEDED(domDoc->GetDoctype(getter_AddRefs(docType))) && docType) {
    return docType->GetName(aDocType);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessibleMixin::GetNameSpaceURIForID(PRInt16 aNameSpaceID, nsAWritableString& aNameSpaceURI)
{
  if (mDocument) {
    nsCOMPtr<nsINameSpaceManager> nameSpaceManager;
    if (NS_SUCCEEDED(mDocument->GetNameSpaceManager(*getter_AddRefs(nameSpaceManager)))) 
      return nameSpaceManager->GetNameSpaceURI(aNameSpaceID, aNameSpaceURI);
  }
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsDocAccessibleMixin::GetDocument(nsIDocument **doc)
{
  *doc = mDocument;
  if (mDocument) {
    NS_IF_ADDREF(*doc);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

