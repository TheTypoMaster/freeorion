// -*- C++ -*-
#ifndef _ModeratorActionsWnd_h_
#define _ModeratorActionsWnd_h_

#include "CUIWnd.h"
#include "CUIControls.h"
#include "../universe/Enums.h"

#include <boost/signal.hpp>

#include <vector>

namespace GG {
    class Button;
}


class ModeratorActionsWnd : public CUIWnd {
public:
    //! \name Structors //!@{
    ModeratorActionsWnd(GG::X w, GG::Y h);
    //!@}

    /** \name Accessors */ //@{
    PlanetType      SelectedPlanetType() const;
    StarType        SelectedStarType() const;
    int             SelectedEmpire() const;
    //!@}

    /** \name Mutators */ //@{
    virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr);
    void            Refresh();
    void            EnableActions(bool enable = true);
    //!@}

    mutable boost::signal<void ()>              ClosingSignal;
    mutable boost::signal<void ()>              NoActionSelectedSignal;
    mutable boost::signal<void (StarType)>      CreateSystemActionSelectedSignal;
    mutable boost::signal<void (PlanetType)>    CreatePlanetActionSelectedSignal;
    mutable boost::signal<void ()>              DeleteObjectActionSelectedSignal;
    mutable boost::signal<void (int)>           SetOwnerActionSelectedSignal;
    mutable boost::signal<void ()>              CreateStarlaneActionSelectedSignal;

private:
    void            DoLayout();
    virtual void    CloseClicked();

    void            NoActionClicked();
    void            CreateSystemClicked();
    void            StarTypeSelected(GG::DropDownList::iterator it);
    void            CreatePlanetClicked();
    void            PlanetTypeSelected(GG::DropDownList::iterator it);
    void            DeleteObjectClicked();
    void            SetOwnerClicked();
    void            EmpireSelected(GG::DropDownList::iterator it);
    void            CreateStarlaneClicked();

    bool                m_actions_enabled;

    GG::Button*         m_no_action_button;
    GG::Button*         m_create_system_button;
    CUIDropDownList*    m_star_type_drop;
    GG::Button*         m_create_planet_button;
    CUIDropDownList*    m_planet_type_drop;
    GG::Button*         m_delete_object_button;
    GG::Button*         m_set_owner_button;
    CUIDropDownList*    m_empire_drop;
    GG::Button*         m_create_starlane_button;
};

#endif // _ModeratorActionsWnd_h_