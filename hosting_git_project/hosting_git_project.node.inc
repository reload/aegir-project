<?php

/**
 * Implementation of hook_node_info().
 */
function hosting_git_project_node_info() {
  #configuration
  $types["git_project"] =  array(
    "type" => 'git_project',
    "name" => 'Git Project',
    "module" => 'hosting_git_project',
    "has_title" => TRUE,
    "title_label" => t('Project Name'),
    "description" => "A git / stub makefile backed project",
    "has_body" => 0, "body_label" => '', "min_word_count" => 0);
  return $types;
}

/**
 * Implementation of hook_load().
 * hosting_name is used by hosting as the name to add after @ when creating
 * the aliasname for the context
 * @param node
 *    Node object
 */
function hosting_git_project_load($node) {
  $additions = db_fetch_object(
    db_query('SELECT makefile_url, project_type, install_profile, base_domain, db_server, web_server, c.name as hosting_name FROM {hosting_git_project} JOIN {hosting_context} c on c.nid = %d WHERE vid = %d', array($node->nid, $node->vid))
  );

  return $additions;
}

/**
 * Implementation of hook_insert().
 */
function hosting_git_project_insert($node) {
  db_query("INSERT INTO {hosting_git_project} (vid, nid, makefile_url, project_type, install_profile, base_domain, db_server, web_server) VALUES (%d, %d, '%s', '%s', '%s', '%s', %d, %d)",
    $node->vid, $node->nid, $node->makefile_url, $node->project_type, $node->install_profile, $node->base_domain, $node->db_server, $node->web_server);

  if (!$node->old_vid) {
    dd("registering context " . preg_replace("/[!\W]/", "", $node->title));
    hosting_context_register($node->nid, 'git_project_' . preg_replace("/[!\W]/", "", $node->title));
  }

  if (!isset($node->no_verify)) {
    dd("verifying");
    hosting_add_task($node->nid, 'verify');
  }
}


/**
 * Implementation of hook_view().
 */
function hosting_git_project_view($node, $teaser = FALSE, $page = FALSE) {
  modalframe_parent_js();

  $node->content['info'] = array(
    '#prefix' => '<div id="hosting-project-info">',
    '#suffix' => '</div>'
  );

  $node->content['info']['makefile_url'] = array(
    '#type' => 'item',
    '#title' => t('Makefile URL'),
    '#value' => filter_xss($node->makefile_url),
    '#weight' => -8
  );

  $node->content['info']['web_server'] = array(
    '#type' => 'item',
    '#title' => t('Web server'),
    '#value' => _hosting_node_link($node->web_server),
    '#weight' => -7
  );

  $node->content['info']['db_server'] = array(
    '#type' => 'item',
      '#title' => t('DB server'),
    '#value' => _hosting_node_link($node->db_server),
    '#weight' => -7
  );

  if ($node->makefile) {
    $node->content['info']['makefile'] = array(
      '#type' => 'item',
      '#title' => t('Drush makefile'),
      '#value' => (preg_match('/^http|^ftp/', $node->makefile)) ? l('makefile', $node->makefile) : filter_xss($node->makefile),
    );
  }

  if ($page) {
    // Task list
    if ($node->nid) {
      $node->content['tasks_view'] = array(
        '#type' => 'item',
        '#value' => hosting_task_table($node),
        '#prefix' => '<div id="hosting-task-list">',
        '#suffix' => '</div>',
        '#weight' => 10,
      );
      $settings['hostingTaskRefresh'] = array(
        'nid' => $node->nid,
        'changed' => $node->changed,
      );
      drupal_add_js($settings, 'setting');

      drupal_add_js(drupal_get_path('module','hosting_task') . '/hosting_task.js');
    }
  }
  return $node;
}

