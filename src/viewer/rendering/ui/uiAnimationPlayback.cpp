// Copyright © 2026 CCP ehf.

#include "uiAnimationPlayback.h"
#include <imgui.h>

#include "uiCustomWidgets.h"

const float ANIMATION_SELECTION_WIDTH = 120.0f;

void UIAnimationPlayback::Render( AppState& appState )
{
	UpdateState( appState );

	const auto& [width, height] = appState.windowSize.GetValue();

	ImGui::SetNextWindowPos( ImVec2( 0, static_cast<float>( height ) - UiConsts::ANIMATION_PLAYER_HEIGHT ), ImGuiCond_Always );
	ImGui::SetNextWindowSize( ImVec2( static_cast<float>( width ), UiConsts::ANIMATION_PLAYER_HEIGHT ), ImGuiCond_Always );
	if( ImGui::Begin( "##animationTitle", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize ) )
	{
		// animation selection
		std::vector<std::string> animationNames;
		std::vector<float> animationDuration;
		const auto& activeAnimationOwner = appState.modelState.activeAnimationOwner.GetValue();
		int32_t selectedAnimationIndex = -1;

		if( activeAnimationOwner != nullptr && !activeAnimationOwner->m_cmfData->animations.empty() )
		{
			// add the rest pose
			animationNames.push_back( "Rest Pose" );
			animationDuration.push_back( 0.0f );
			const auto& animations = activeAnimationOwner->m_cmfData->animations;
			std::for_each( animations.begin(), animations.end(), [&animationNames, &animationDuration]( const cmf::Animation& animation ) {
				animationDuration.push_back( animation.duration );
				animationNames.push_back( cmf::ToStdString( animation.name ) );
			} );
			// Map currentAnimation to index: 0 = Rest Pose (""), 1..N = animations[0..N-1]
			if( appState.modelState.currentAnimation.GetValue().empty() )
			{
				selectedAnimationIndex = 0;
			}
			else
			{
				auto it = std::find_if( animations.begin(), animations.end(), [&appState]( const cmf::Animation& animation ) {
					return cmf::ToStdString( animation.name ) == appState.modelState.currentAnimation.GetValue();
				} );
				if( it != animations.end() )
				{
					selectedAnimationIndex = static_cast<int>( std::distance( animations.begin(), it ) ) + 1;
				}
				else
				{
					// couldn't find the animation, reset to rest pose
					selectedAnimationIndex = 0;
					m_playing = false;
					m_currentTime = 0.0f;
					m_duration = 0.0f;
					appState.modelState.currentAnimation.SetValue( "" );
					appState.modelState.currentAnimationTime.SetValue( m_currentTime );
				}
			}
		}

		ImGui::BeginDisabled( animationNames.empty() );

		ImGui::PushItemWidth( ANIMATION_SELECTION_WIDTH );
		ImGui::ComboBox( "##animation", animationNames, selectedAnimationIndex, [&]( int32_t selectedIndex ) {
			m_playing = false;
			m_currentTime = 0.0f;
			m_duration = animationDuration[selectedIndex];
			if( selectedIndex == 0 )
			{
				// rest pose
				appState.modelState.currentAnimation.SetValue( "" );
			}
			else
			{
				appState.modelState.currentAnimation.SetValue( animationNames[selectedIndex] );
			}
			appState.modelState.currentAnimationTime.SetValue( m_currentTime );
		} );
		ImGui::PopItemWidth();
		ImGui::SetItemTooltip( "Select an animation to play" );

		ImGui::SameLine();

		// step left
		if( ImGui::FontAwesomeButton( ICON_FA_CHEVRON_LEFT, 0 ) )
		{
			m_currentTime -= 0.1f;
		}
		ImGui::SameLine();

		// play/pause button
		if( ImGui::FontAwesomeButton( m_playing ? ICON_FA_PAUSE : ICON_FA_PLAY, 1 ) )
		{
			m_playing = !m_playing;

			if( m_currentTime == m_duration )
			{
				m_currentTime = 0.0f;
			}
		}

		ImGui::SameLine();

		// step right
		if( ImGui::FontAwesomeButton( ICON_FA_CHEVRON_RIGHT, 2 ) )
		{
			m_currentTime += 0.1f;
		}

		ImGui::SameLine();
		bool repeatPressed = m_repeat ? ImGui::FontAwesomeButton( ICON_FA_REPEAT, 3 ) : ImGui::FontAwesomeSlashedButton( ICON_FA_REPEAT, 3 );
		if( repeatPressed )
		{
			m_repeat = !m_repeat;
		}
		ImGui::SetItemTooltip( "Repeat the animation" );

		ImGui::SameLine();
		float availableWidth = ImGui::GetContentRegionAvail().x;
		ImGui::PushItemWidth( availableWidth );
		if( ImGui::SliderFloat( "##playbackSlider", &m_currentTime, 0.0f, m_duration ) )
		{
			appState.modelState.currentAnimationTime.SetValue( m_currentTime );
		}
		ImGui::SetItemTooltip( "%.3fs / %.3fs", m_currentTime, m_duration );
		ImGui::PopItemWidth();

		ImGui::EndDisabled();
		ImGui::SameLine();
	}
	ImGui::End();
}

void UIAnimationPlayback::UpdateState( AppState& appState )
{
	if( m_playing )
	{
		m_currentTime += ImGui::GetIO().DeltaTime;
		if( m_currentTime >= m_duration )
		{
			if( m_repeat )
			{
				m_currentTime = 0.0f;
			}
			else
			{
				m_currentTime = m_duration;
				m_playing = false;
			}
		}
	}
	appState.modelState.currentAnimationTime.SetValue( m_currentTime );
}