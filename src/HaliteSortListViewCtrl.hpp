
#pragma once

#include <boost/array.hpp>
#include <boost/signals.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include "stdAfx.hpp"
#include "halTorrent.hpp"
#include "WinAPIWaitableTimer.hpp"

template<class T>
class UpdateLock
{
public:
	UpdateLock(T& window) :
		window_(window)
	{
		++window_.updateLock_;
		window_.SetRedraw(false);
	}
	
	~UpdateLock()
	{
		if (!--window_.updateLock_)
			unlock();
	}
	
	void unlock()
	{
		window_.SetRedraw(true);
		window_.InvalidateRect(NULL, true);
	}
	
private:
	T& window_;
};

template <class TBase, typename adapterType, size_t N=-1>
class CHaliteSortListViewCtrl : 
	public CSortListViewCtrlImpl<CHaliteSortListViewCtrl<TBase, adapterType, N> >
{
public:
	typedef CHaliteSortListViewCtrl<TBase, adapterType, N> thisClass;
	typedef CSortListViewCtrlImpl<thisClass> parentClass;
	
	class selection_manager : 
		private boost::noncopyable
	{	
	public:
		selection_manager(thisClass& m_list) :
			m_list_(m_list)
		{}
		
		typedef std::wstring string_t; 
		typedef const string_t& param_type;
		
		void sync_list(bool list_to_manager, bool signal_change=true)
		{
			if (list_to_manager)
			{	
				if (listToManager() && signal_change) signal();
			}
			else
			{
				if (managerToList() && signal_change) signal();
			}
		}
		
		bool listToManager()
		{
			boost::array<wchar_t, MAX_PATH> pathBuffer;
			std::set<string_t> all_selected;
			string_t selected = L"";
			
			bool do_signal = false;
			
			int total = m_list_.GetItemCount();
			
			for (int i=0; i<total; ++i)
			{
				UINT flags = m_list_.GetItemState(i, LVIS_SELECTED);
				
				if (flags && LVIS_SELECTED)
				{
					m_list_.GetItemText(i, 0, pathBuffer.c_array(), pathBuffer.size());	
					all_selected.insert(pathBuffer.data());
				}
				if (flags && LVIS_FOCUSED)
				{
					selected = pathBuffer.data();
				}
			}

			if (all_selected != all_selected_)
			{
				std::swap(all_selected_, all_selected);
				do_signal = true;
			}
					
			if (selected_ != selected)
			{
				std::swap(selected_, selected);
				do_signal = true;
			}
			
			return do_signal;
		}
		
		bool managerToList()
		{
			// Prevent changing states from signaling another sync
			UpdateLock<thisClass> lock(m_list_);
			
			boost::array<wchar_t, MAX_PATH> pathBuffer;
			LV_FINDINFO findInfo = { sizeof(LV_FINDINFO) }; 
			findInfo.flags = LVFI_STRING;
			
			bool do_signal = true; // Always signal for now!
			
			int total = m_list_.GetItemCount();
			
			for (int i=0; i<total; ++i)
			{
				m_list_.GetItemText(i, 0, pathBuffer.c_array(), pathBuffer.size());
				string_t temp_name = pathBuffer.data();
				
				LVITEM lvi = { LVIF_STATE };
				lvi.state = 0;
				lvi.stateMask = LVIS_SELECTED|LVIS_FOCUSED;
				
				if (temp_name == selected_)
				{
					lvi.state |= LVIS_FOCUSED;
				}
				if (all_selected_.find(temp_name) != all_selected_.end())
				{
					lvi.state |= LVIS_SELECTED;
				}
				
				m_list_.SetItemState(i, &lvi);
			}			
			
			return do_signal;
		}
		
		int selectMatch(const string_t& name)
		{
			LV_FINDINFO findInfo = { sizeof(LV_FINDINFO) }; 
			findInfo.flags = LVFI_STRING;
					
			findInfo.psz = name.c_str();
			
			int itemPos = m_list_.FindItem(&findInfo, -1);	
						
			if (itemPos == -1)
				return itemPos;
				
			UINT flags = m_list_.GetItemState(itemPos, LVIS_SELECTED);
			
			//if (!flags && LVIS_SELECTED)
			{
				LVITEM lvi = { LVIF_STATE };
				lvi.state = LVIS_SELECTED;
				lvi.stateMask = LVIS_SELECTED;
				m_list_.SetItemState(itemPos, &lvi);
			}
		
			return itemPos;
		}
		
		param_type selected() const { return selected_; }
		
		int selectedIndex()
		{
			LV_FINDINFO findInfo = { sizeof(LV_FINDINFO) }; 	
			findInfo.psz = selected_.c_str();
			
			return m_list_.FindItem(&findInfo, -1);		
		}
		
		const std::set<string_t>& allSelected() const { return all_selected_; }
		
		void setSelected(const string_t& sel) 
		{
			selected_ = sel;
		}
		
		void setSelected(int itemPos)
		{		
			hal::event().post(shared_ptr<hal::EventDetail>(new hal::EventDebug(hal::Event::info, (wformat(L"Set Selected %1%") % itemPos).str().c_str())));

			LVITEM lvi = { LVIF_STATE };
			lvi.state = LVIS_SELECTED|LVIS_FOCUSED;
			lvi.stateMask = LVIS_SELECTED|LVIS_FOCUSED;
			m_list_.SetItemState(itemPos, &lvi);
			
			m_list_.SetSelectionMark(itemPos);
			
			sync_list(true);
		}
		
		void clear()
		{
			// Prevent changing states from signaling another sync
			UpdateLock<thisClass> lock(m_list_);
			
//			hal::event().post(shared_ptr<hal::EventDetail>(new hal::EventDebug(hal::Event::info, (wformat(L"Clear")).str().c_str())));
	
			m_list_.DeleteItem(selectedIndex());
			
			sync_list(true);	
		}
		
		void clear_all_selected()
		{
			// Prevent changing states from signaling another sync
			UpdateLock<thisClass> lock(m_list_);
			
//			hal::event().post(shared_ptr<hal::EventDetail>(new hal::EventDebug(hal::Event::info, (wformat(L"ClearAllSelected")).str().c_str())));

			int total = m_list_.GetItemCount();
			
			for (int i=total-1; i>=0; --i)
			{
				UINT flags = m_list_.GetItemState(i, LVIS_SELECTED);
				
				if (flags && LVIS_SELECTED)
					m_list_.DeleteItem(i);
			}
			all_selected_.clear();
			
			sync_list(true);	
		}
		
		void clear_all()
		{
			// Prevent changing states from signaling another sync
			UpdateLock<thisClass> lock(m_list_);
			
//			hal::event().post(shared_ptr<hal::EventDetail>(new hal::EventDebug(hal::Event::info, (wformat(L"ClearAll")).str().c_str())));

			m_list_.DeleteAllItems();
			all_selected_.clear();
			
			sync_list(true);		
		}
		
		void attach(boost::function<void (param_type)> fn) const { selection_.connect(fn); }
		
		void signal() 
		{ 
			selection_(selected_); 
		}
		
	private:
		string_t selected_;
		std::set<string_t> all_selected_;
		
		mutable boost::signal<void (param_type)> selection_;
		thisClass& m_list_;
	};
	
	class CHaliteHeaderCtrl : public CWindowImpl<CHaliteHeaderCtrl, CHeaderCtrl>
	{
	public:
		BEGIN_MSG_MAP(CHaliteHeaderCtrl)
			REFLECTED_NOTIFY_CODE_HANDLER(NM_RCLICK, OnRClick)
		END_MSG_MAP()
		
		void Attach(HWND hWndNew)
		{
			ATLASSERT(::IsWindow(hWndNew));
			CWindowImpl<CHaliteHeaderCtrl, CHeaderCtrl>::SubclassWindow(hWndNew);

			menu_.CreatePopupMenu();
			
			MENUITEMINFO minfo = {sizeof(MENUITEMINFO)};
			minfo.fMask = MIIM_STRING|MIIM_ID|MIIM_FTYPE|MIIM_STATE;
			minfo.fType = MFT_STRING;
			minfo.fState = MFS_CHECKED;
			minfo.dwTypeData = L"Hello";
			
			menu_.InsertMenuItem(0, false, &minfo);
//			TBase* pT = static_cast<TBase*>(this);
//			pT->OnAttach();
		}
		
		LRESULT OnRClick(int i, LPNMHDR pnmh, BOOL&)
		{
			POINT ptPoint;
			GetCursorPos(&ptPoint);
			menu_.TrackPopupMenu(0, ptPoint.x, ptPoint.y, m_hWnd);

			return 0;
		}
		
	private:
		WTL::CMenu menu_;
	};
	
	class Adapter
	{
	public:
		virtual bool less(adapterType& l, adapterType& r) = 0;
		virtual std::wstring print(adapterType& t) = 0;
	};

public:
	typedef selection_manager SelectionManager;
	typedef SelectionManager selection_manager_class;
	
	thisClass() :
		manager_(*this),
		updateLock_(0)
	{
		if (TBase::LISTVIEW_ID_MENU)
		{
			BOOL menu_created = menu_.LoadMenu(TBase::LISTVIEW_ID_MENU);
			assert(menu_created);	
		}

		wstring column_names = hal::app().res_wstr(TBase::LISTVIEW_ID_COLUMNNAMES);
		boost::split(names_, column_names, boost::is_any_of(L";"));
		
		wstring column_widths = hal::app().res_wstr(TBase::LISTVIEW_ID_COLUMNWIDTHS);
		std::vector<wstring> widths;
		boost::split(widths, column_widths, boost::is_any_of(L";"));
				
		listColumnWidth_.reserve(names_.size());	
		listColumnOrder_.reserve(names_.size());
		
		for (size_t i=0; i<names_.size(); ++i)
		{
			listColumnWidth_.push_back(lexical_cast<int>(widths[i]));
			listColumnOrder_.push_back(i);
		}	
	}

	BEGIN_MSG_MAP_EX(thisClass)
		REFLECTED_NOTIFY_CODE_HANDLER(NM_CLICK, OnClick)
		REFLECTED_NOTIFY_CODE_HANDLER(NM_RCLICK, OnRClick)
		REFLECTED_NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnItemChanged)
	//	REFLECTED_NOTIFY_CODE_HANDLER(LVN_COLUMNCLICK , OnColClick)

		DEFAULT_REFLECTION_HANDLER()
		CHAIN_MSG_MAP(parentClass)
	END_MSG_MAP()

	void Attach(HWND hWndNew)
	{
		ATLASSERT(::IsWindow(hWndNew));
        parentClass::SubclassWindow(hWndNew);

		TBase* pT = static_cast<TBase*>(this);
		pT->OnAttach();
	}
	
	void SetListViewDetails()
	{
		vectorSizePreConditions();
		
		header_.Attach(this->GetHeader());
		header_.ModifyStyle(0, HDS_DRAGDROP|HDS_FULLDRAG);
			
		foreach (wstring name, names_)
		{
			int i = header_.GetItemCount();
			
			AddColumn(name.c_str(), i);
			SetColumnSortType(i, LVCOLSORT_CUSTOM);
		}		

		for (unsigned i=0; i<names_.size(); ++i)
			SetColumnWidth(i, listColumnWidth_[i]);
		
		SetColumnOrderArray(names_.size(), &listColumnOrder_[0]);	
	}
	
	template<std::size_t Size>
	void SetDefaults(array<int, Size> a)
	{
		assert (Size == names_.size());
		vectorSizePreConditions();
		
		for (size_t i=0; i<names_.size(); ++i)
		{
			listColumnWidth_[i] = a[i];
			listColumnOrder_[i] = i;
		}		
	}
	
	// Should probably make this redundant!!
	void GetListViewDetails()
	{
		vectorSizePreConditions();
		
		GetColumnOrderArray(names_.size(), &listColumnOrder_[0]);
		
		for (size_t i=0; i<names_.size(); ++i)
			listColumnWidth_[i] = GetColumnWidth(i);	
	}

	LRESULT OnClick(int, LPNMHDR pnmh, BOOL&)
	{
	//	manager().sync_list(true);

		return 0;
	}

	LRESULT OnItemChanged(int, LPNMHDR pnmh, BOOL&)
	{		
		if (canUpdate()) 
		{
			manager_.sync_list(true, true);
//			if (syncTimer_.reset(200, bind(&thisClass::syncTimeout, this)))	
//				hal::event().post(shared_ptr<hal::EventDetail>(new hal::EventDebug(hal::Event::info, (wformat(L"********** Set")).str().c_str())));
		}
		
		return 0;
	}

	LRESULT OnRClick(int i, LPNMHDR pnmh, BOOL&)
	{
		hal::event().post(shared_ptr<hal::EventDetail>(new hal::EventDebug(hal::Event::info, (wformat(L"RClick %1%") % pnmh->code).str().c_str())));
		LPNMITEMACTIVATE pia = (LPNMITEMACTIVATE)pnmh;
		manager_.sync_list(true);
		
		if (TBase::LISTVIEW_ID_MENU)
		{
			assert (menu_.IsMenu());
			CMenuHandle sMenu = menu_.GetSubMenu(0);
			assert (sMenu.IsMenu());
	
			POINT ptPoint;
			GetCursorPos(&ptPoint);
			sMenu.TrackPopupMenu(0, ptPoint.x, ptPoint.y, m_hWnd);
		}

		return 0;
	}

	LRESULT OnColClick(int i, LPNMHDR pnmh, BOOL&)
	{
		LPNMLISTVIEW pnlv = (LPNMLISTVIEW)pnmh;
		
		MessageBox((lexical_cast<wstring>(pnlv->iSubItem)).c_str(), L"Hi", 0);
		return 0;
	}
		
	int CompareItemsCustom(LVCompareParam* pItem1, LVCompareParam* pItem2, int iSortCol)
	{
		TBase* pT = static_cast<TBase*>(this);
		return pT->CompareItemsCustom(pItem1, pItem2, iSortCol);
	}
	
	friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & boost::serialization::make_nvp("width", listColumnWidth_);
        ar & boost::serialization::make_nvp("order", listColumnOrder_);
    }

	const SelectionManager& manager() { return manager_; }
		
	std::vector<int>& listColumnWidth() { return listColumnWidth_; }
	std::vector<int>& listColumnOrder() { return listColumnOrder_; }
	
	const std::vector<int>& listColumnWidth() const { return listColumnWidth_; }
	const std::vector<int>& listColumnOrder() const { return listColumnOrder_; }
	
	bool canUpdate() const { return updateLock_ == 0; }
	
	void clearFocused() { manager_.clear(); }
	void clearSelected() { manager_.clear_all_selected(); }
	void clearAll() { manager_.clear(); }

protected:
	SelectionManager manager_;
	boost::ptr_vector<Adapter> adapters_;
	
private:
	void vectorSizePreConditions()
	{
		if (listColumnWidth_.size() != names_.size())
		{
			listColumnWidth_.clear();
			listColumnWidth_.insert(listColumnWidth_.end(), names_.size(), 50);	
		}
		
		if (listColumnOrder_.size() != names_.size())
		{		
			listColumnOrder_.clear();
			listColumnOrder_.insert(listColumnOrder_.end(), names_.size(), 0);
		}		
	}
	
	void syncTimeout()
	{
		hal::event().post(shared_ptr<hal::EventDetail>(new hal::EventDebug(hal::Event::info, (wformat(L"********** Signaled")).str().c_str())));
		
		manager_.sync_list(true, true);
	}
		
	WTL::CMenu menu_;
	CHaliteHeaderCtrl header_;
	std::vector<wstring> names_;
	std::vector<int> listColumnWidth_;
	std::vector<int> listColumnOrder_;
	
	int updateLock_;
	friend class UpdateLock<thisClass>;
	
	WinAPIWaitableTimer syncTimer_;
};
