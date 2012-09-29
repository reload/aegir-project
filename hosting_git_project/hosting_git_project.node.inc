<?php
/**
 * @file
 * Project and deployment nodes.
 */

define('GIT_PROJECT_TYPE', 'git_project');
define('GIT_PROJECT_DEPLOYMENT_TYPE', 'git_project_deployment');

/**
 * Implements of hook_node_info().
 */
function hosting_git_project_node_info() {
  $types["git_project"] = array(
    "type" => GIT_PROJECT_TYPE,
    "name" => 'Git Project',
    "module" => 'hosting_git_project',
    "has_title" => TRUE,
    "title_label" => t('Project Name'),
    "description" => "A git / stub makefile backed project",
    "has_body" => 0, "body_label" => '', "min_word_count" => 0);

  $types["git_project_deployment"] = array(
    "type" => GIT_PROJECT_DEPLOYMENT_TYPE,
    "name" => 'Git Project Deployment',
    "module" => 'hosting_git_project',
    "has_title" => TRUE,
    "title_label" => t('Project Name'),
    "description" => "A Git Project deployment",
    "has_body" => 0, "body_label" => '', "min_word_count" => 0);

  return $types;
}

/**
 * Ensure we only load projects.
 */
function hosting_git_project_node_load($arg) {
  if (!is_numeric($arg)) {
    return FALSE;
  }
  if ($node = node_load($arg)) {
    if ($node->type == 'git_project') {
      return $node;
    }
  }
  return FALSE;
}


/**
 * Ensure we only load deployments.
 */
function hosting_git_project_deployment_node_load($arg) {
  if (!is_numeric($arg)) {
    return FALSE;
  }
  if ($node = node_load($arg)) {
    if ($node->type == 'git_project_deployment') {
      return $node;
    }
  }
  return FALSE;
}


/**
 * Implements of hook_load().
 *
 * hosting_name is used by hosting as the name to add after @ when creating
 * the aliasname for the context.
 */
function hosting_git_project_load($node) {
  $additions = array();

  if ($node->type == GIT_PROJECT_TYPE) {
    $additions = db_fetch_object(
      db_query('SELECT makefile_url, project_type, install_profile, domain_basename, db_server, web_server, project_status, c.name as hosting_name FROM {hosting_git_project} JOIN {hosting_context} c on c.nid = %d WHERE vid = %d', array($node->nid, $node->vid))
    );
  }

  elseif ($node->type == GIT_PROJECT_DEPLOYMENT_TYPE) {
    $additions = db_fetch_object(
      db_query('SELECT git_project, deployment_name, deployment_branch, deployment_tag, deployment_revision, deploy_status, site_nid, platform_nid, c.name as hosting_name FROM {hosting_git_project_deployment} JOIN {hosting_context} c on c.nid = %d WHERE vid = %d', array($node->nid, $node->vid))
    );
  }

  return $additions;
}

/**
 * Implements of hook_insert().
 */
function hosting_git_project_insert($node) {
  if ($node->type == GIT_PROJECT_TYPE) {
    db_query("INSERT INTO {hosting_git_project} (vid, nid, makefile_url, project_type, install_profile, domain_basename, db_server, web_server, project_status) VALUES (%d, %d, '%s', '%s', '%s', '%s', %d, %d, %d)",
      $node->vid, $node->nid, $node->makefile_url, $node->project_type, $node->install_profile, $node->domain_basename, $node->db_server, $node->web_server, $node->project_status);

    // First insert: register this node as a context instance.
    if (!$node->old_vid) {
      hosting_context_register($node->nid, strtolower('git_project_' . preg_replace("/[!\W]/", "", $node->title)));
    }

    // Then kick off a verification.
    if (!isset($node->no_verify)) {
      hosting_add_task($node->nid, 'verify');
    }
  }
  elseif ($node->type == GIT_PROJECT_DEPLOYMENT_TYPE) {
    db_query("INSERT INTO {hosting_git_project_deployment} (vid, nid, git_project, deployment_name, deployment_branch, deployment_tag, deployment_revision, deploy_status, site_nid, platform_nid) VALUES (%d, %d, %d, '%s', '%s', '%s', '%s', '%s', %d, %d)",
      $node->vid, $node->nid,
      $node->git_project,
      $node->deployment_name,
      $node->deployment_branch,
      $node->deployment_tag,
      $node->deployment_revision,
      $node->deploy_status,
      $node->site_nid,
      $node->platform_nid
      );

    // First insert: register this node as a context instance.
    if (!$node->old_vid) {
      // TODO - ... and if the project is not there?.
      // Load project.
      $project = node_load($node->git_project);

      // Alias should be <project_name>_<deployment name>.
      $deployment_alias = str_replace('git_project_', '', $project->hosting_name) . '_' . preg_replace("/[!\W]/", "", $node->title);

      // Prefixed with git_project_deployment_.
      $deployment_alias = strtolower('git_project_deployment_'  . $deployment_alias);

      hosting_context_register($node->nid, $deployment_alias);
    }

    // Then kick off a verification.
    if (!isset($node->no_verify)) {
      hosting_add_task($node->nid, 'verify');
    }
  }
}

/**
 * Implements of hook_view().
 */
function hosting_git_project_view($node, $teaser = FALSE, $page = FALSE) {
  if ($node->type == GIT_PROJECT_TYPE) {
    // View project page.
    modalframe_parent_js();

    $node->content['info'] = array(
      '#prefix' => '<div id="hosting-platform-info">',
      '#suffix' => '</div>',
    );

    $node->content['info']['makefile_url'] = array(
      '#type' => 'item',
      '#title' => t('Makefile URL'),
      '#value' => filter_xss($node->makefile_url),
      '#weight' => -9 ,
    );

    $node->content['info']['domain_basename'] = array(
      '#type' => 'item',
      '#title' => t('Domain basename'),
      '#value' => filter_xss($node->domain_basename),
      '#weight' => -8,
    );

    $node->content['info']['web_server'] = array(
      '#type' => 'item',
      '#title' => t('Web server'),
      '#value' => _hosting_node_link($node->web_server),
      '#weight' => -7,
    );

    $node->content['info']['db_server'] = array(
      '#type' => 'item',
      '#title' => t('DB server'),
      '#value' => _hosting_node_link($node->db_server),
      '#weight' => -6,
    );

    if ($node->makefile) {
      $node->content['info']['makefile'] = array(
        '#type' => 'item',
        '#title' => t('Drush makefile'),
        '#value' => (preg_match('/^http|^ftp/', $node->makefile)) ? l(t('makefile'), $node->makefile) : filter_xss($node->makefile),
      );
    }

    // Task list if we're on a page
    if ($page) {
      if ($node->nid) {
        // Node has been created.

        // Task list.
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
        drupal_add_js(drupal_get_path('module', 'hosting_task') . '/hosting_task.js');

        // Deployment list
        $node->content['deployment_list'] = array(
          '#value' => hosting_git_deployment_deployment_list($node->nid),
          '#weight' => 20,
        );

      }
    }
  }
  elseif ($node->type == GIT_PROJECT_DEPLOYMENT_TYPE) {
    modalframe_parent_js();

    $node->content['info'] = array(
      '#prefix' => '<div id="hosting-platform-info">',
      '#suffix' => '</div>',
    );

    $node->content['info']['deployment_branch'] = array(
      '#type' => 'item',
      '#title' => t('Branch'),
      '#value' => filter_xss($node->deployment_branch ? $node->deployment_branch : '&gt;none&pt;'),
      '#weight' => -7,
    );

    $node->content['info']['deployment_tag'] = array(
      '#type' => 'item',
      '#title' => t('Tag'),
      '#value' => filter_xss($node->deployment_tag ? $node->deployment_tag : t('(none)')),
      '#weight' => -6,
    );

    $node->content['info']['deployment_revision'] = array(
      '#type' => 'item',
      '#title' => t('Revision'),
      '#value' => filter_xss($node->deployment_revision ? $node->deployment_revision : t('(none)')),
      '#weight' => -5,
    );

    $node->content['info']['site_nid'] = array(
      '#type' => 'item',
      '#title' => t('Site'),
      '#value' => $node->site_nid ? _hosting_node_link($node->site_nid) : t('(none)'),
      '#weight' => -4,
    );

    $node->content['info']['site_nid'] = array(
      '#type' => 'item',
      '#title' => t('Site'),
      '#value' => $node->platform_nid ? _hosting_node_link($node->platform_nid) : t('(none)'),
      '#weight' => -4,
    );

    $node->content['info']['git_project'] = array(
      '#type' => 'item',
      '#title' => t('Project'),
      '#value' => $node->git_project ? _hosting_node_link($node->git_project) : t('(none)'),
      '#weight' => -3,
    );

    if ($page) {
      // Task list.
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

        drupal_add_js(drupal_get_path('module', 'hosting_task') . '/hosting_task.js');
      }
    }
  }
  return $node;
}



function _git_project_render_deploy_list($project_node){
    foreach ($queues as $key => $info) {
    $row = array();
    $row[] = drupal_render($form[$key]['enabled']);
    $row[] = drupal_render($form[$key]['description']);
    $row[] = drupal_render($form[$key]['frequency']);
    $row[] = drupal_render($form[$key]['last_run']);
    $rows[] = $row;
  }
  $output = theme('table', $header, $rows);
}
