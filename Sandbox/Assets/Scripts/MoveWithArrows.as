class MoveWithArrows : ScriptBehaviour
{
	float speed = 5.0f;
	void OnUpdate(float dt)
	{
		Vector3 pos = self.GetPosition();

		if (Input::IsKeyDown(KeyCode::Right)) pos.x += speed * dt;
		if (Input::IsKeyDown(KeyCode::Left))  pos.x -= speed * dt;
		if (Input::IsKeyDown(KeyCode::Up))    pos.z += speed * dt;
		if (Input::IsKeyDown(KeyCode::Down))  pos.z -= speed * dt;

		self.SetPosition(pos);
	}
}
