<?php

/**
 * @file
 * Development related functions.
 */

/**
 * Generate profile2
 */
function profile2_generate_form($form, &$form_state) {
  $types = profile2_get_types();
  foreach ($types as $id => $type) {
    $types[$id] = $type->label;
  }

  $form['profile2_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Profile types'),
    '#description' => t('Select profile types to create profile. If no types are selected, profile2 will be generated for all types.'),
    '#options' => $types,
  );

  $roles['[any]'] = t('any');
  $roles += user_roles(TRUE);
  unset($roles[DRUPAL_AUTHENTICATED_RID]); // Don't list authorized role.

  $form['profile2_roles_select'] = array(
    '#type' => 'select',
    '#title' => t('The roles of user'),
    '#default_value' => -1,
    '#options' => $roles,
  );

  $form['profile2_generate_limit_number'] = array(
    '#type' => 'textfield',
    '#title' => t('Maximum number of profiles per type.'),
    '#default_value' => 50,
    '#size' => 10,
  );

  $form['profile2_kill'] = array(
    '#type' => 'checkbox',
    '#title' => t('Replace existing profiles.'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Generate'),
  );
  return $form;
}

/**
 * FormAPI submission to generate nodes.
 */
function profile2_generate_form_submit($form, &$form_state) {
  $user_ids = array();
  if ($form_state['values']['profile2_roles_select'] == '[any]') {
    // generate profile2 for all user.
    $users = user_load_multiple(FALSE);
    //don't generate profile2 for anonymous user (uid = 0).
    unset($users[0]);
    $user_ids = array_keys($users);
  }
  else {
    $role = $form_state['values']['profile2_roles_select'];
    $query = db_select('users', 'u')->fields('u', array('uid'));
    $query->condition('u.uid', 0, '<>');
    $users_roles_alias = $query->join('users_roles', 'ur', '%alias.uid = u.uid');
    $query->condition($users_roles_alias . '.rid', $role);

    $users = $query->execute()->fetchCol('uid');
    foreach ($users as $u) {
      $user_ids[$u] = $u;
    }
  }

  $types = $form_state['values']['profile2_types'];
  $kill = $form_state['values']['profile2_kill'];
  $limit_number = $form_state['values']['profile2_generate_limit_number'];

  if (count($user_ids) > 0) {
    $types = array_keys(array_filter($types));
    $pids = profile2_generate_profile($user_ids, $types, $kill, $limit_number);
    drupal_set_message(t("Generated %count profile(s).", array('%count' => count($pids))));
  }
}

/**
 * The main API function for creating profile2.
 */
function profile2_generate_profile($uids, $types, $kill, $limit_number) {
  if ($kill) {
    // get all profile2 and delete this.
    $profiles = profile2_load_multiple(FALSE);
    $pids = array_keys($profiles);
    if ($pids) {
      // delete all profile2 first.
      profile2_delete_multiple($pids);
    }
  }

  $new_pids = array();
  while (count($new_pids) < $limit_number && $type = array_shift($types)) {
    while(count($new_pids) < $limit_number && $uid = array_shift($uids)) {
      if (profile2_load_by_user($uid, $type)) {
        continue;
      }
      $profile2 = entity_create('profile2', array('type' => $type, 'uid' => $uid));
      // Populate all core fields on behalf of field.module
      module_load_include('inc', 'devel_generate', 'devel_generate.fields');
      module_load_include('inc', 'devel_generate', 'devel_generate');
      // add language for profile2 object.
      // @see devel_generate_fields
      $profile2->language = LANGUAGE_NONE;
      devel_generate_fields($profile2, 'profile2', $profile2->type);
      profile2_save($profile2);
      $new_pids[] = $profile2->pid;
    }
  }

  return $new_pids;
}
