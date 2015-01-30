/*
	liblinphone_tester - liblinphone test suite
	Copyright (C) 2014  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "liblinphone_tester.h"
#include "linphonecore.h"
#include "belle-sip/belle-sip.h"

static void call_multicast_base(bool_t video) {
	LinphoneCoreManager* marie = linphone_core_manager_new( "marie_rc");
	LinphoneCoreManager* pauline = linphone_core_manager_new( "pauline_rc");
	int begin;
	int leaked_objects;
	LinphoneVideoPolicy marie_policy, pauline_policy;

	belle_sip_object_enable_leak_detector(TRUE);
	begin=belle_sip_object_get_object_count();

	if (video) {
		linphone_core_enable_video_capture(marie->lc, TRUE);
		linphone_core_enable_video_display(marie->lc, TRUE);
		linphone_core_enable_video_capture(pauline->lc, TRUE);
		linphone_core_enable_video_display(pauline->lc, FALSE);

		marie_policy.automatically_initiate=TRUE;
		marie_policy.automatically_accept=TRUE;
		pauline_policy.automatically_initiate=TRUE;
		pauline_policy.automatically_accept=TRUE;

		linphone_core_set_video_policy(marie->lc,&marie_policy);
		linphone_core_set_video_policy(pauline->lc,&pauline_policy);
		linphone_core_set_video_multicast_addr(pauline->lc,"224.1.2.3");
		linphone_core_enable_video_multicast(pauline->lc,TRUE);
	}
	linphone_core_set_audio_multicast_addr(pauline->lc,"224.1.2.3");
	linphone_core_enable_audio_multicast(pauline->lc,TRUE);

	CU_ASSERT_TRUE(call(pauline,marie));
	wait_for_until(marie->lc, pauline->lc, NULL, 1, 3000);
	if (linphone_core_get_current_call(marie->lc)) {
		CU_ASSERT_TRUE(linphone_call_get_audio_stats(linphone_core_get_current_call(marie->lc))->download_bandwidth>70);
		if (video) {
			/*check video path*/
			linphone_call_set_next_video_frame_decoded_callback(linphone_core_get_current_call(marie->lc),linphone_call_cb,marie->lc);
			linphone_call_send_vfu_request(linphone_core_get_current_call(marie->lc));
			CU_ASSERT_TRUE( wait_for(marie->lc,pauline->lc,&marie->stat.number_of_IframeDecoded,1));
		}

		end_call(marie,pauline);
	}
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);

	leaked_objects=belle_sip_object_get_object_count()-begin;
	CU_ASSERT_TRUE(leaked_objects==0);
	if (leaked_objects>0){
		belle_sip_object_dump_active_objects();
	}

}
static void call_multicast(void)  {
	call_multicast_base(FALSE);
}
static void multicast_audio_with_pause_resume() {
	call_paused_resumed_base(TRUE);
}
#ifdef VIDEO_ENABLED
static void call_multicast_video(void)  {
	call_multicast_base(TRUE);
}
#endif
static void early_media_with_multicast_base(bool_t video) {
	LinphoneCoreManager* marie   = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager* pauline = linphone_core_manager_new("pauline_rc");
	MSList* lcs = NULL;
	LinphoneCall* marie_call;
	int dummy=0;
	int leaked_objects;
	int begin;
	LinphoneVideoPolicy marie_policy, pauline_policy;

	belle_sip_object_enable_leak_detector(TRUE);
	begin=belle_sip_object_get_object_count();

	if (video) {
		linphone_core_enable_video_capture(pauline->lc, TRUE);
		linphone_core_enable_video_display(pauline->lc, TRUE);
		linphone_core_enable_video_capture(marie->lc, TRUE);
		linphone_core_enable_video_display(marie->lc, FALSE);

		marie_policy.automatically_initiate=TRUE;
		marie_policy.automatically_accept=TRUE;
		pauline_policy.automatically_initiate=TRUE;
		pauline_policy.automatically_accept=TRUE;

		linphone_core_set_video_policy(marie->lc,&marie_policy);
		linphone_core_set_video_policy(pauline->lc,&pauline_policy);
		linphone_core_set_video_multicast_addr(marie->lc,"224.1.2.3");
		linphone_core_enable_video_multicast(marie->lc,TRUE);
	}
	linphone_core_set_audio_multicast_addr(marie->lc,"224.1.2.3");
	linphone_core_enable_audio_multicast(marie->lc,TRUE);


	lcs = ms_list_append(lcs,marie->lc);
	lcs = ms_list_append(lcs,pauline->lc);
	/*
		Marie calls Pauline, and after the call has rung, transitions to an early_media session
	*/

	marie_call = linphone_core_invite_address(marie->lc, pauline->identity);

	CU_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphoneCallIncomingReceived,1,3000));
	CU_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallOutgoingRinging,1,1000));


	if (linphone_core_inc_invite_pending(pauline->lc)) {
		/* send a 183 to initiate the early media */
		if (video) {
			/*check video path*/
			linphone_call_set_next_video_frame_decoded_callback(linphone_core_get_current_call(pauline->lc),linphone_call_cb,pauline->lc);
		}
		linphone_core_accept_early_media(pauline->lc, linphone_core_get_current_call(pauline->lc));

		CU_ASSERT_TRUE( wait_for_list(lcs, &pauline->stat.number_of_LinphoneCallIncomingEarlyMedia,1,2000) );
		CU_ASSERT_TRUE( wait_for_list(lcs, &marie->stat.number_of_LinphoneCallOutgoingEarlyMedia,1,2000) );

		wait_for_list(lcs, &dummy, 1, 3000);

		CU_ASSERT_TRUE(linphone_call_get_audio_stats(linphone_core_get_current_call(pauline->lc))->download_bandwidth>70);

		if (video) {
			CU_ASSERT_TRUE( wait_for(marie->lc,pauline->lc,&pauline->stat.number_of_IframeDecoded,1));
		}

		linphone_core_accept_call(pauline->lc, linphone_core_get_current_call(pauline->lc));

		CU_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallConnected, 1,1000));
		CU_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallStreamsRunning, 1,1000));

		end_call(marie,pauline);
	}
	ms_free(lcs);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);

	leaked_objects=belle_sip_object_get_object_count()-begin;
	CU_ASSERT_TRUE(leaked_objects==2/*fixme jehan*/);
	if ((leaked_objects)>0){
		belle_sip_object_dump_active_objects();
	}
}

static void early_media_with_multicast_audio() {
	early_media_with_multicast_base(FALSE);
}
static void unicast_incoming_with_multicast_audio_on() {
	simple_call_base(TRUE);
}
#ifdef VIDEO_ENABLED
static void early_media_with_multicast_video() {
	early_media_with_multicast_base(TRUE);
}
#endif

test_t multicast_call_tests[] = {
	{ "Multicast audio call",call_multicast},
	{ "Multicast call with pause/resume",multicast_audio_with_pause_resume},
	{ "Early media multicast audio call",early_media_with_multicast_audio},
	{ "Unicast incoming call with multicast activated",unicast_incoming_with_multicast_audio_on},
#ifdef VIDEO_ENABLED
	{ "Multicast video call",call_multicast_video},
	{ "Early media multicast video call",early_media_with_multicast_video},
#endif
};

test_suite_t multicast_call_test_suite = {
	"Multicast Call",
	NULL,
	NULL,
	sizeof(multicast_call_tests) / sizeof(multicast_call_tests[0]),
	multicast_call_tests
};
