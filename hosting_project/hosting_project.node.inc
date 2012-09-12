<?php

/**
 * Implementation of hook_node_info().
 */
function hosting_project_node_info() {
  #configuration
  $types["project"] =  array(
    "type" => 'project',
    "name" => 'Project',
    "module" => 'hosting_project',
    "has_title" => TRUE,
    "title_label" => t('Project Name'),
    "description" => "A git / stub makefile backed project",
    "has_body" => 0, "body_label" => '', "min_word_count" => 0);
  return $types;
}


/**
 * hook_nodeapi()
 */
function hosting_project_nodeapi(&$node, $op, $a3 = null) {

  if ($node->type == 'platform'){
    // On insert of a platform, determine project and save to hosting_devshop_project_objects
    // @TODO: This is how we save the project path at the moment because
    //  the platform node is coming from hosting-import...
    if ($op == 'insert') {
      $path = explode('/', $node->publish_path);
      $environment = array_pop($path);
      $project_path = implode('/', $path);

      $project = devshop_project_load_by_path($project_path);

      // Save to table
      db_query('INSERT INTO {hosting_devshop_project_object} (project_nid, object_nid, object_type, env_type) VALUES (%d, %d, "%s", "%s")', $project->nid, $node->nid, $node->type, $environment);
    }
  }

  //Platforms and Sites
  if ($node->type == 'platform' || $node->type == 'site'){

    // Load Project info
    if ($op == 'load'){
      $data = db_fetch_array(db_query('SELECT project_nid, env_type AS project_environment FROM {hosting_devshop_project_object} WHERE object_nid = %d', $node->nid));
      return $data;
    }

    // Display Project info
    if ($op == 'view'){
      $project = node_load($node->project_nid);
      $node->content['info']['project'] = array(
  '#type' => 'item',
  '#title' => t('Project'),
  '#value' => l($project->title, "node/$project->nid"),
  '#weight' => -12
      );
       $node->content['info']['env'] = array(
  '#type' => 'item',
  '#title' => t('Environment Type'),
  '#value' => $node->project_environment,
  '#weight' => -11
      );

      return $data;
    }
  }
}
