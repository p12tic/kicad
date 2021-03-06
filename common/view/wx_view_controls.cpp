/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 Torsten Hueter, torstenhtr <at> gmx.de
 * Copyright (C) 2013 CERN
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <wx/wx.h>

#include <view/view.h>
#include <view/wx_view_controls.h>
#include <gal/graphics_abstraction_layer.h>
#include <tool/tool_dispatcher.h>

using namespace KIGFX;

const wxEventType WX_VIEW_CONTROLS::EVT_REFRESH_MOUSE = wxNewEventType();

WX_VIEW_CONTROLS::WX_VIEW_CONTROLS( VIEW* aView, wxWindow* aParentPanel ) :
    VIEW_CONTROLS( aView ),
    m_state( IDLE ),
    m_parentPanel( aParentPanel )
{
    m_parentPanel->Connect( wxEVT_MOTION,
                            wxMouseEventHandler( WX_VIEW_CONTROLS::onMotion ), NULL, this );
    m_parentPanel->Connect( wxEVT_MOUSEWHEEL,
                            wxMouseEventHandler( WX_VIEW_CONTROLS::onWheel ), NULL, this );
    m_parentPanel->Connect( wxEVT_MIDDLE_UP,
                            wxMouseEventHandler( WX_VIEW_CONTROLS::onButton ), NULL, this );
    m_parentPanel->Connect( wxEVT_MIDDLE_DOWN,
                            wxMouseEventHandler( WX_VIEW_CONTROLS::onButton ), NULL, this );
    m_parentPanel->Connect( wxEVT_LEFT_UP,
                            wxMouseEventHandler( WX_VIEW_CONTROLS::onButton ), NULL, this );
    m_parentPanel->Connect( wxEVT_LEFT_DOWN,
                            wxMouseEventHandler( WX_VIEW_CONTROLS::onButton ), NULL, this );
#if defined _WIN32 || defined _WIN64
    m_parentPanel->Connect( wxEVT_ENTER_WINDOW,
                            wxMouseEventHandler( WX_VIEW_CONTROLS::onEnter ), NULL, this );
#endif

    m_panTimer.SetOwner( this );
    this->Connect( wxEVT_TIMER,
                   wxTimerEventHandler( WX_VIEW_CONTROLS::onTimer ), NULL, this );
}


void VIEW_CONTROLS::ShowCursor( bool aEnabled )
{
    m_view->GetGAL()->SetCursorEnabled( aEnabled );
}


void WX_VIEW_CONTROLS::onMotion( wxMouseEvent& aEvent )
{
    m_mousePosition.x = aEvent.GetX();
    m_mousePosition.y = aEvent.GetY();

    if( m_forceCursorPosition )
        m_cursorPosition = m_view->ToScreen( m_forcedPosition );
    else if( m_snappingEnabled )
        m_cursorPosition = m_view->GetGAL()->GetGridPoint( m_mousePosition );
    else
        m_cursorPosition = m_mousePosition;

    bool isAutoPanning = false;

    if( m_autoPanEnabled )
    {
        isAutoPanning = handleAutoPanning( aEvent );
    }

    if( !isAutoPanning && aEvent.Dragging() )
    {
        if( m_state == DRAG_PANNING )
        {
            VECTOR2D   d = m_dragStartPoint - m_mousePosition;
            VECTOR2D   delta = m_view->ToWorld( d, false );

            m_view->SetCenter( m_lookStartPoint + delta );
            aEvent.StopPropagation();
        }
        else
        {
            aEvent.Skip();
        }
    }
}


void WX_VIEW_CONTROLS::onWheel( wxMouseEvent& aEvent )
{
    const double wheelPanSpeed = 0.001;

    if( aEvent.ControlDown() || aEvent.ShiftDown() )
    {
        // Scrolling
        VECTOR2D scrollVec = m_view->ToWorld( m_view->GetScreenPixelSize() *
                             ( (double) aEvent.GetWheelRotation() * wheelPanSpeed ), false );
        double   scrollSpeed;

        if( abs( scrollVec.x ) > abs( scrollVec.y ) )
            scrollSpeed = scrollVec.x;
        else
            scrollSpeed = scrollVec.y;

        VECTOR2D delta( aEvent.ControlDown() ? -scrollSpeed : 0.0,
                        aEvent.ShiftDown() ? -scrollSpeed : 0.0 );

        m_view->SetCenter( m_view->GetCenter() + delta );
    }
    else
    {
        // Zooming
        wxLongLong  timeStamp   = wxGetLocalTimeMillis();
        double      timeDiff    = timeStamp.ToDouble() - m_timeStamp.ToDouble();

        m_timeStamp = timeStamp;
        double zoomScale;

        // Set scaling speed depending on scroll wheel event interval
        if( timeDiff < 500 && timeDiff > 0 )
        {
            zoomScale = ( aEvent.GetWheelRotation() > 0.0 ) ? 2.05 - timeDiff / 500 :
                        1.0 / ( 2.05 - timeDiff / 500 );
        }
        else
        {
            zoomScale = ( aEvent.GetWheelRotation() > 0.0 ) ? 1.05 : 0.95;
        }

        VECTOR2D anchor = m_view->ToWorld( VECTOR2D( aEvent.GetX(), aEvent.GetY() ) );
        m_view->SetScale( m_view->GetScale() * zoomScale, anchor );
    }

    aEvent.Skip();
}


void WX_VIEW_CONTROLS::onButton( wxMouseEvent& aEvent )
{
    switch( m_state )
    {
    case IDLE:
    case AUTO_PANNING:
        if( aEvent.MiddleDown() )
        {
            m_dragStartPoint = VECTOR2D( aEvent.GetX(), aEvent.GetY() );
            m_lookStartPoint = m_view->GetCenter();
            m_state = DRAG_PANNING;
        }

        if( aEvent.LeftUp() )
        {
            m_state = IDLE;     // Stop autopanning when user release left mouse button
        }

        break;

    case DRAG_PANNING:
        if( aEvent.MiddleUp() )
        {
            m_state = IDLE;
        }

        break;
    }

    aEvent.Skip();
}


void WX_VIEW_CONTROLS::onEnter( wxMouseEvent& aEvent )
{
    m_parentPanel->SetFocus();
}


void WX_VIEW_CONTROLS::onTimer( wxTimerEvent& aEvent )
{
    switch( m_state )
    {
    case AUTO_PANNING:
    {
        double borderSize = std::min( m_autoPanMargin * m_view->GetScreenPixelSize().x,
                                      m_autoPanMargin * m_view->GetScreenPixelSize().y );

        VECTOR2D dir( m_panDirection );

        if( dir.EuclideanNorm() > borderSize )
            dir = dir.Resize( borderSize );

        dir = m_view->ToWorld( dir, false );
        m_view->SetCenter( m_view->GetCenter() + dir * m_autoPanSpeed );

        // Notify tools that the cursor position has changed in the world coordinates
        wxCommandEvent moveEvent( EVT_REFRESH_MOUSE );
        wxPostEvent( m_parentPanel, moveEvent );
    }
    break;

    case IDLE:    // Just remove unnecessary warnings
    case DRAG_PANNING:
        break;
    }
}


void WX_VIEW_CONTROLS::SetGrabMouse( bool aEnabled )
{
    m_grabMouse = aEnabled;

    if( aEnabled )
        m_parentPanel->CaptureMouse();
    else
        m_parentPanel->ReleaseMouse();
}


const VECTOR2D WX_VIEW_CONTROLS::GetMousePosition() const
{
    wxPoint msp = wxGetMousePosition();
    wxPoint winp = m_parentPanel->GetScreenPosition();

    return VECTOR2D( msp.x - winp.x, msp.y - winp.y );
}


const VECTOR2D WX_VIEW_CONTROLS::GetCursorPosition() const
{
    if( m_snappingEnabled )
        return m_view->GetGAL()->GetGridPoint( GetMousePosition() );
    else
        return GetMousePosition();
}


bool WX_VIEW_CONTROLS::handleAutoPanning( const wxMouseEvent& aEvent )
{
    VECTOR2D p( aEvent.GetX(), aEvent.GetY() );

    // Compute areas where autopanning is active
    double borderStart = std::min( m_autoPanMargin * m_view->GetScreenPixelSize().x,
                                   m_autoPanMargin * m_view->GetScreenPixelSize().y );
    double borderEndX = m_view->GetScreenPixelSize().x - borderStart;
    double borderEndY = m_view->GetScreenPixelSize().y - borderStart;

    m_panDirection = VECTOR2D();

    if( p.x < borderStart )
        m_panDirection.x = -( borderStart - p.x );
    else if( p.x > borderEndX )
        m_panDirection.x = ( p.x - borderEndX );

    if( p.y < borderStart )
        m_panDirection.y = -( borderStart - p.y );
    else if( p.y > borderEndY )
        m_panDirection.y = ( p.y - borderEndY );

    bool borderHit = ( m_panDirection.x != 0 || m_panDirection.y != 0 );

    switch( m_state )
    {
    case AUTO_PANNING:
        if( !borderHit )
        {
            m_panTimer.Stop();
            m_state = IDLE;

            return false;
        }

        return true;
        break;

    case IDLE:
        if( borderHit )
        {
            m_state = AUTO_PANNING;
            m_panTimer.Start( (int) ( 1000.0 / 60.0 ) );

            return true;
        }

        return false;
        break;

    case DRAG_PANNING:
        return false;
    }

    wxASSERT_MSG( false, wxT( "This line should never be reached" ) );
    return false;    // Should not be reached, just avoid the compiler warnings..
}
